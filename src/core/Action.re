open OptUtil.Syntax;

type tile_shape =
  | Num
  | NumLit(int)
  | Var(Var.t)
  | Paren
  | Lam
  | Ap
  | Ann
  | Plus
  | Arrow;

type t =
  | Mark
  | Move(Direction.t)
  | Delete(Direction.t)
  | Construct(tile_shape);

module Typ = {
  let tile_of_shape = (~selection=[]): (tile_shape => option(HTyp.Tile.t)) =>
    fun
    | NumLit(_)
    | Lam
    | Ap
    | Plus
    | Var(_)
    | Ann => None
    | Num => Some(Operand(Num))
    | Paren =>
      Some(Operand(Paren(selection == [] ? HTyp.mk_hole() : selection)))
    | Arrow => Some(BinOp(Arrow));
};
module Pat = {
  let tile_of_shape = (~selection=[]): (tile_shape => option(HPat.Tile.t)) =>
    fun
    | Num
    | NumLit(_)
    | Lam
    | Ap
    | Plus
    | Arrow => None
    | Var(x) => Some(Operand(Var(x)))
    | Paren =>
      Some(Operand(Paren(selection == [] ? HPat.mk_hole() : selection)))
    // TODO come back to Ann, consider generalizing output to list of tiles
    // so that bidelimited wrapping is not the only possible outcome on a selection
    | Ann => Some(PostOp(Ann(NotInHole, HTyp.mk_hole())));
};
module Exp = {
  let tile_of_shape = (~selection=[]): (tile_shape => option(HExp.Tile.t)) =>
    fun
    | Num
    | Ann
    | Arrow => None
    | NumLit(n) => Some(Operand(Num(NotInHole, n)))
    | Var(x) => Some(Operand(Var(NotInHole, x)))
    | Paren =>
      Some(Operand(Paren(selection == [] ? HExp.mk_hole() : selection)))
    // TODO come back to Lam, consider generalizing output to list of tiles
    // so that bidelimited wrapping is not the only possible outcome on a selection
    | Lam => Some(PreOp(Lam(NotInHole, HPat.mk_hole())))
    | Ap =>
      Some(
        PostOp(Ap(NotInHole, selection == [] ? HExp.mk_hole() : selection)),
      )
    | Plus => Some(BinOp(Plus(NotInHole)));
};

let rec perform = (a: t, edit_state: EditState.t): option(EditState.t) =>
  switch (a, edit_state) {
  | (Move(d), (mode, zipper)) =>
    let+ (focus, did_it_zip) = {
      let move = path =>
        switch (zipper) {
        | `Exp(zipper) => (
            ZPath.Exp.move(d, path, zipper) :>
              option((ZPath.t, EditState.Zipper.did_it_zip))
          )
        | `Pat(zipper) => (
            ZPath.Pat.move(d, path, zipper) :>
              option((ZPath.t, EditState.Zipper.did_it_zip))
          )
        | `Typ(zipper) => (
            ZPath.Typ.move(d, path, zipper) :>
              option((ZPath.t, EditState.Zipper.did_it_zip))
          )
        };
      move(EditState.Mode.get_focus(mode));
    };
    let mode = EditState.Mode.put_focus(focus, mode);
    switch (did_it_zip) {
    | None => (mode, zipper)
    | Some((two_step, zipped)) => (
        EditState.Mode.update_anchors(ZPath.cons(two_step), mode),
        zipped,
      )
    };

  | (Mark, (Normal(focus), zipper)) =>
    Some((Selecting((focus, focus)), zipper))

  | (Delete(d), (Normal(focus), zipper)) =>
    switch (zipper) {
    | `Typ(z) =>
      let+ (selection, did_it_zip) = ZPath.Typ.select(d, focus, z);
      let zipper =
        switch (did_it_zip) {
        | None => zipper
        | Some((_, zipped)) => (zipped :> EditState.Zipper.t)
        };
      (EditState.Mode.Selecting(selection), zipper);
    | `Pat(z) =>
      let+ (selection, did_it_zip) = ZPath.Pat.select(d, focus, z);
      let zipper =
        switch (did_it_zip) {
        | None => zipper
        | Some((_, zipped)) => (zipped :> EditState.Zipper.t)
        };
      (EditState.Mode.Selecting(selection), zipper);
    | `Exp(z) =>
      let+ (selection, did_it_zip) = ZPath.Exp.select(d, focus, z);
      let zipper =
        switch (did_it_zip) {
        | None => zipper
        | Some((_, zipped)) => (zipped :> EditState.Zipper.t)
        };
      (EditState.Mode.Selecting(selection), zipper);
    }

  | (Construct(_), (Normal(([two_step, ...steps], j)), zipper)) =>
    let mode = EditState.Mode.Normal((steps, j));
    let zipper =
      switch (zipper) {
      | `Typ(zipper) => (
          ZPath.Typ.unzip(two_step, zipper) :> EditState.Zipper.t
        )
      | `Pat(zipper) => (
          ZPath.Pat.unzip(two_step, zipper) :> EditState.Zipper.t
        )
      | `Exp(zipper) => (
          ZPath.Exp.unzip(two_step, zipper) :> EditState.Zipper.t
        )
      };
    perform(a, (mode, zipper));
  | (Construct(s), (Normal(([], j)), zipper)) =>
    switch (zipper) {
    | `Typ(ty, unzipped) =>
      let+ tile = Typ.tile_of_shape(s);
      let (j, ty) = {
        let (prefix, suffix) = ListUtil.split_n(j, ty);
        let (prefix, suffix) =
          HTyp.fix_empty_holes(prefix @ [tile], suffix);
        (List.length(prefix), prefix @ suffix);
      };
      (EditState.Mode.Normal(([], j)), `Typ((ty, unzipped)));
    | `Pat(p, unzipped) =>
      let* tile = Pat.tile_of_shape(s);
      let (j, p) = {
        let (prefix, suffix) = ListUtil.split_n(j, p);
        let (prefix, suffix) =
          HPat.fix_empty_holes(prefix @ [tile], suffix);
        (List.length(prefix), prefix @ suffix);
      };
      let+ zipper =
        switch (unzipped) {
        | None =>
          let (p, _, _) = Statics.Pat.syn_fix_holes(Ctx.empty, p);
          Some(`Pat((p, None)));
        | Some(ztile) =>
          let+ ZInfo.Pat.{ctx, mode} = ZInfo.Pat.mk_ztile(ztile);
          let (p, ztile) =
            switch (mode) {
            | Syn({fix}) =>
              let (p, ty, ctx) = Statics.Pat.syn_fix_holes(ctx, p);
              (p, fix(ty, ctx));
            | Ana({expected, fix}) =>
              let (p, ctx) = Statics.Pat.ana_fix_holes(ctx, p, expected);
              (p, fix(ctx));
            };
          `Pat((p, Some(ztile)));
        };
      (EditState.Mode.Normal(([], j)), zipper);
    | `Exp(e, unzipped) =>
      let* tile = Exp.tile_of_shape(s);
      let (j, e) = {
        let (prefix, suffix) = ListUtil.split_n(j, e);
        let (prefix, suffix) =
          HExp.fix_empty_holes(prefix @ [tile], suffix);
        (List.length(prefix), prefix @ suffix);
      };
      let+ zipper =
        switch (unzipped) {
        | None =>
          let (e, _) = Statics.Exp.syn_fix_holes(Ctx.empty, e);
          Some(`Exp((e, None)));
        | Some(ztile) =>
          let+ ZInfo.Exp.{ctx, mode} = ZInfo.Exp.mk_ztile(ztile);
          let (e, ztile) =
            switch (mode) {
            | Syn({fn_pos, fix}) =>
              let (e, ty) = Statics.Exp.syn_fix_holes(~fn_pos, ctx, e);
              (e, fix(ty));
            | Ana({expected, fix}) =>
              let e = Statics.Exp.ana_fix_holes(ctx, e, expected);
              (e, fix());
            };
          `Exp((e, Some(ztile)));
        };
      (EditState.Mode.Normal(([], j)), zipper);
    }

  | (
      Delete(_) | Construct(_),
      (
        Selecting((
          ([two_step_l, ...steps_l], j_l),
          ([two_step_r, ...steps_r], j_r),
        )),
        zipper,
      ),
    )
      when two_step_l == two_step_r =>
    let mode = EditState.Mode.Selecting(((steps_l, j_l), (steps_r, j_r)));
    let zipper =
      switch (zipper) {
      | `Typ(zipper) => (
          ZPath.Typ.unzip(two_step_l, zipper) :> EditState.Zipper.t
        )
      | `Pat(zipper) => (
          ZPath.Pat.unzip(two_step_l, zipper) :> EditState.Zipper.t
        )
      | `Exp(zipper) => (
          ZPath.Exp.unzip(two_step_l, zipper) :> EditState.Zipper.t
        )
      };
    perform(a, (mode, zipper));

  | (Mark | Delete(_), (Selecting((l, r) as selected), zipper)) =>
    let c = ZPath.compare(l, r);
    if (c == 0) {
      Some((Normal(r), zipper));
    } else if (c > 0) {
      perform(a, (Selecting((r, l)), zipper));
    } else {
      let rounded =
        switch (zipper) {
        | `Typ(ty, _) => ZPath.Typ.round_selection(selected, ty)
        | `Pat(p, _) => ZPath.Pat.round_selection(selected, p)
        | `Exp(e, _) => ZPath.Exp.round_selection(selected, e)
        };
      Some((Restructuring(rounded, fst(rounded)), zipper));
    };

  | (Construct(_), (Selecting(_), _)) =>
    // TODO
    None

  | (Construct(_), (Restructuring(_), _)) => None

  | (Delete(_), (Restructuring(selection, _), zipper)) =>
    switch (zipper) {
    | `Typ(ty, unzipped) =>
      let+ (path, ty) = ZPath.Typ.remove_selection(selection, ty);
      (EditState.Mode.Normal(path), `Typ((ty, unzipped)));
    | `Pat(p, unzipped) =>
      let+ (path, p) = ZPath.Pat.remove_selection(selection, p);
      (EditState.Mode.Normal(path), `Pat((p, unzipped)));
    | `Exp(e, unzipped) =>
      let+ (path, e) = ZPath.Exp.remove_selection(selection, e);
      (EditState.Mode.Normal(path), `Exp((e, unzipped)));
    }

  | (Mark, (Restructuring(selection, target), zipper)) =>
    switch (zipper) {
    | `Typ(ty, unzipped) =>
      let+ (path, ty) = ZPath.Typ.restructure(selection, target, ty);
      (EditState.Mode.Normal(path), `Typ((ty, unzipped)));
    | `Pat(p, unzipped) =>
      let+ (path, p) = ZPath.Pat.restructure(selection, target, p);
      (EditState.Mode.Normal(path), `Pat((p, unzipped)));
    | `Exp(e, unzipped) =>
      let+ (path, e) = ZPath.Exp.restructure(selection, target, e);
      (EditState.Mode.Normal(path), `Exp((e, unzipped)));
    }
  };