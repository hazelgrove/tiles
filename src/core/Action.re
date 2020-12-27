open Sexplib.Std;
open Util;
open OptUtil.Syntax;

[@deriving sexp]
type tile_shape =
  | Num
  | Bool
  | NumLit(int)
  | Var(Var.t)
  | Paren
  | Lam
  | Let
  | Ap
  | Ann
  | Plus
  | Arrow;

[@deriving sexp]
type t =
  | Mark
  | Move(Direction.t)
  | Delete(Direction.t)
  | Construct(tile_shape);

module Common =
       (
         T: Tile.S,
         M: {
           let tile_of_shape: (~selection: T.s=?, tile_shape) => option(T.t);
         },
       ) => {
  module Ts = Tiles.Make(T);

  let construct = (s: tile_shape, j: ZPath.caret_step, ts: T.s) => {
    let+ tile = M.tile_of_shape(s);
    let (prefix, suffix) = ListUtil.split_n(j, ts);
    let (prefix, tile, suffix) =
      ListUtil.take_3(Ts.fix_empty_holes([prefix, [tile], suffix]));
    (List.length(prefix), prefix @ tile @ suffix);
  };
};

module Typ = {
  module M = {
    let tile_of_shape = (~selection=[]): (tile_shape => option(HTyp.T.t)) =>
      fun
      | NumLit(_)
      | Lam
      | Let
      | Ap
      | Plus
      | Var(_)
      | Ann => None
      | Num => Some(Op(Num))
      | Bool => Some(Op(Bool))
      | Paren =>
        Some(Op(Paren(selection == [] ? HTyp.mk_hole() : selection)))
      | Arrow => Some(Bin(Arrow));
  };
  include Common(HTyp.T, M);
};
module Pat = {
  module M = {
    let tile_of_shape = (~selection=[]): (tile_shape => option(HPat.T.t)) =>
      fun
      | Num
      | Bool
      | NumLit(_)
      | Lam
      | Let
      | Ap
      | Plus
      | Arrow => None
      | Var(x) => Some(Op(Var(x)))
      | Paren =>
        Some(Op(Paren(selection == [] ? HPat.mk_hole() : selection)))
      // TODO come back to Ann, consider generalizing output to list of tiles
      // so that bidelimited wrapping is not the only possible outcome on a selection
      | Ann => Some(Post(Ann(NotInHole, HTyp.mk_hole())));
  };
  include Common(HPat.T, M);
};
module Exp = {
  module M = {
    let tile_of_shape = (~selection=[]): (tile_shape => option(HExp.T.t)) =>
      fun
      | Num
      | Bool
      | Ann
      | Arrow => None
      | NumLit(n) => Some(Op(Num(NotInHole, n)))
      | Var(x) => Some(Op(Var(NotInHole, x)))
      | Paren =>
        Some(Op(Paren(selection == [] ? HExp.mk_hole() : selection)))
      // TODO come back to Lam, consider generalizing output to list of tiles
      // so that bidelimited wrapping is not the only possible outcome on a selection
      | Lam => Some(Pre(Lam(NotInHole, HPat.mk_hole())))
      | Let => Some(Pre(Let(HPat.mk_hole(), HExp.mk_hole())))
      | Ap =>
        Some(
          Post(Ap(NotInHole, selection == [] ? HExp.mk_hole() : selection)),
        )
      | Plus => Some(Bin(Plus(NotInHole)));
  };
  include Common(HExp.T, M);
};

let rec move_selecting =
        (
          d: Direction.t,
          {anchor, focus, _} as selection: ZPath.anchored_selection,
          zipper: EditState.Zipper.t,
        )
        : option((ZPath.anchored_selection, EditState.Zipper.t)) => {
  let anchor_sort = EditState.Zipper.sort_at(anchor, zipper);
  let (_, focus_side) = ZPath.mk_ordered_selection(selection);
  let* (next_focus, did_it_zip) = EditState.Zipper.move(d, focus, zipper);
  let (selection, zipper) =
    switch (did_it_zip) {
    | None => ({...selection, focus: next_focus}, zipper)
    | Some((two_step, zipper)) => (
        {
          ...ZPath.cons_anchored_selection(two_step, selection),
          focus: next_focus,
        },
        zipper,
      )
    };
  let next_sort = EditState.Zipper.sort_at(next_focus, zipper);
  let c = Sort.compare(next_sort, anchor_sort);
  if ((anchor == focus || d == focus_side) && c > 0) {
    let+ (selection, zipper) =
      move_selecting(
        Direction.toggle(d),
        {...selection, anchor: selection.focus, focus: selection.anchor},
        zipper,
      );
    (
      {...selection, anchor: selection.focus, focus: selection.anchor},
      zipper,
    );
  } else if ((anchor == focus || d == focus_side) && c < 0) {
    move_selecting(d, selection, zipper);
  } else if (d != focus_side && c < 0) {
    let rec move_until_same_sort = anchor => {
      // assuming movement should never cause zip up here, review assumption
      let (next_anchor, _) =
        EditState.Zipper.move(Direction.toggle(d), anchor, zipper)
        |> OptUtil.get(() => assert(false));
      if (Sort.compare(
            EditState.Zipper.sort_at(next_anchor, zipper),
            next_sort,
          )
          == 0) {
        next_anchor;
      } else {
        move_until_same_sort(next_anchor);
      };
    };
    let next_anchor = move_until_same_sort(selection.anchor);
    Some(({...selection, anchor: next_anchor}, zipper));
  } else {
    Some((selection, zipper));
  };
};

let rec perform_normal =
        (a: t, (steps, j) as focus: ZPath.t, zipper: EditState.Zipper.t)
        : option(EditState.t) =>
  switch (a) {
  | Mark => Some((Selecting({origin: focus, anchor: focus, focus}), zipper))

  | Move(d) =>
    let+ (focus, did_it_zip) = EditState.Zipper.move(d, focus, zipper);
    let mode = EditState.Mode.Normal(focus);
    switch (did_it_zip) {
    | None => (mode, zipper)
    | Some((two_step, zip_result)) => (
        EditState.Mode.update_anchors(ZPath.cons(two_step), mode),
        zip_result,
      )
    };

  | Delete(d) =>
    let+ (selection, zipper) =
      move_selecting(d, {origin: focus, anchor: focus, focus}, zipper);
    let ((l, _) as selection, _) = ZPath.mk_ordered_selection(selection);
    (EditState.Mode.Restructuring(selection, l), zipper);

  | Construct(s) =>
    switch (steps) {
    | [two_step, ...steps] =>
      let zipper = EditState.Zipper.unzip(two_step, zipper);
      perform_normal(a, (steps, j), zipper);
    | [] =>
      switch (zipper) {
      | `Typ(ty, unzipped) =>
        let+ (j, ty) = Typ.construct(s, j, ty);
        (EditState.Mode.Normal(([], j)), `Typ((ty, unzipped)));
      | `Pat(p, unzipped) =>
        let* (j, p) = Pat.construct(s, j, p);
        let+ zipper =
          switch (unzipped) {
          | None =>
            let (p, _, _) = Statics.Pat.syn_fix_holes(Ctx.empty, p);
            Some(`Pat((p, None)));
          | Some(ztile) =>
            let+ ZInfo.Pat.{ctx, mode} = ZInfo.Pat.mk_ztile(ztile);
            let (p, ztile) =
              switch (mode) {
              | Syn(fix) =>
                let (p, ty, ctx) = Statics.Pat.syn_fix_holes(ctx, p);
                (p, fix(ty, ctx));
              | Ana(expected, fix) =>
                let (p, ctx) = Statics.Pat.ana_fix_holes(ctx, p, expected);
                (p, fix(ctx));
              | Let_pat(def_ty, fix) =>
                let (p, pty, _) = Statics.Pat.syn_fix_holes(ctx, p);
                let joined = PType.join_or_to_type(pty, def_ty);
                let ctx =
                  Statics.Pat.ana(ctx, p, joined)
                  |> OptUtil.get(() =>
                       failwith(
                         "expected joined type to be consistent with p",
                       )
                     );
                (p, fix(pty, ctx));
              };
            `Pat((p, Some(ztile)));
          };
        (EditState.Mode.Normal(([], j)), zipper);
      | `Exp(e, unzipped) =>
        let* (j, e) = Exp.construct(s, j, e);
        let+ zipper =
          switch (unzipped) {
          | None =>
            let (e, _) = Statics.Exp.syn_fix_holes(Ctx.empty, e);
            Some(`Exp((e, None)));
          | Some(ztile) =>
            let+ ZInfo.Exp.{ctx, mode} = ZInfo.Exp.mk_ztile(ztile);
            let (e, ztile) =
              switch (mode) {
              | Syn(fix) =>
                let (e, ty) = Statics.Exp.syn_fix_holes(ctx, e);
                (e, fix(ty));
              | Ana(expected, fixed) =>
                let e = Statics.Exp.ana_fix_holes(ctx, e, expected);
                (e, fixed);
              | Fn_pos(fix) =>
                let (e, ty) = Statics.Exp.syn_fix_holes(ctx, e);
                switch (Type.matched_arrow(ty)) {
                | None =>
                  let e = HExp.put_hole_status(InHole, e);
                  (e, fix(Hole, Hole));
                | Some((ty_in, ty_out)) => (e, fix(ty_in, ty_out))
                };
              | Let_def(_) => failwith("todo")
              };
            `Exp((e, Some(ztile)));
          };
        (EditState.Mode.Normal(([], j)), zipper);
      }
    }
  };

let perform_selecting =
    (a: t, selection: ZPath.anchored_selection, zipper: EditState.Zipper.t)
    : option(EditState.t) =>
  switch (a) {
  | Delete(_)
  | Mark =>
    let (ordered, _) = ZPath.mk_ordered_selection(selection);
    Some((Restructuring(ordered, fst(ordered)), zipper));

  | Move(d) =>
    let+ (selection, zipper) = move_selecting(d, selection, zipper);
    (EditState.Mode.Selecting(selection), zipper);

  | Construct(_) =>
    // TODO
    None
  };

let perform_restructuring =
    (
      a: t,
      (l, r) as selection: ZPath.ordered_selection,
      (steps_t, j_t) as target: ZPath.t,
      zipper: EditState.Zipper.t,
    )
    : option(EditState.t) =>
  switch (a) {
  | Mark =>
    let+ (path, zipper) =
      EditState.Zipper.restructure(selection, target, zipper);
    (EditState.Mode.Normal(path), zipper);
  | Move(d) =>
    let ((steps_l, j_l), (steps_r, j_r)) = (l, r);
    // assuming now that restructuring mode is only
    // entered when selection contains unmatched delim
    let j_t = j_t + Direction.sign(d);
    if (steps_t == steps_l
        && 0 <= j_t
        && j_t <= j_l
        || steps_t == steps_r
        && j_r <= j_t
        && j_t <= EditState.Zipper.length_at(steps_r, zipper)) {
      Some((Restructuring(selection, (steps_t, j_t)), zipper));
    } else if (steps_t == steps_l && j_t > j_l) {
      Some((Restructuring(selection, r), zipper));
    } else if (steps_t == steps_r && j_t < j_r) {
      Some((Restructuring(selection, l), zipper));
    } else {
      None;
    };
  | Delete(_) =>
    let+ (path, zipper) =
      EditState.Zipper.delete_selection(selection, zipper);
    (EditState.Mode.Normal(path), zipper);
  | Construct(_) => None
  };

let perform = (a: t, (mode, zipper): EditState.t): option(EditState.t) =>
  switch (mode) {
  | Normal(focus) => perform_normal(a, focus, zipper)
  | Selecting(selection) => perform_selecting(a, selection, zipper)
  | Restructuring(selection, target) =>
    perform_restructuring(a, selection, target, zipper)
  };
