type tile_shape =
  | Num(int)
  | Var(Var.t)
  | Paren
  | Lam
  | Ap
  | Ann
  | Plus
  | Arrow;

type t =
  // Move(direction)
  // Mark
  | Delete
  | Construct(tile_shape);

module Typ = {
  let tile_of_shape: tile_shape => option(HTyp.Tile.t) =
    fun
    | Num(_)
    | Lam
    | Ap
    | Plus
    | Var(_)
    | Ann => None
    | Paren => Some(Paren(HTyp.OperandHole))
    | Arrow => Some(Arrow);

  let rec perform = (a: t, zty: ZTyp.t): option(ZTyp.t) =>
    switch (zty) {
    | Z(prefix, suffix) =>
      switch (a) {
      | Delete =>
        switch (prefix) {
        | [] => None
        | [t, ...prefix] =>
          let (tiles, n) =
            HTyp.delete_tile_and_fix_empty_holes(prefix, t, suffix);
          let (new_prefix, new_suffix) = ListUtil.split_n(n, tiles);
          Some(Z(new_prefix, new_suffix));
        }
      | Construct(s) =>
        switch (tile_of_shape(s)) {
        | None => None
        | Some(tile) =>
          let (tiles, n) =
            HTyp.insert_tile_and_fix_empty_holes(prefix, tile, suffix);
          let (new_prefix, new_suffix) = ListUtil.split_n(n, tiles);
          Some(Z(new_prefix, new_suffix));
        }
      }
    | ParenZ(zbody) =>
      perform(a, zbody) |> Option.map(zbody => ZTyp.ParenZ(zbody))
    };
};

module Pat = {
  let tile_of_shape: tile_shape => option(HPat.Tile.t) =
    fun
    | Num(_)
    | Lam
    | Ap
    | Plus
    | Arrow => None
    | Var(x) => Some(Var(x))
    | Paren => Some(Paren(HPat.OperandHole))
    | Ann => Some(Ann(NotInHole, HTyp.OperandHole));

  let syn_fix_and_split = (ctx, n, tiles) => {
    let (new_p, ty, ctx) =
      Statics.Pat.syn_fix_holes(ctx, HPat.parse(tiles));
    (ListUtil.split_n(n, HPat.Tile.unparse(new_p)), ty, ctx);
  };
  let ana_fix_and_split = (ctx, n, tiles, ty) => {
    let (new_p, ctx) =
      Statics.Pat.ana_fix_holes(ctx, HPat.parse(tiles), ty);
    (ListUtil.split_n(n, HPat.Tile.unparse(new_p)), ctx);
  };

  let rec syn_perform =
          (ctx: Ctx.t, a: t, zp: ZPat.t): option((ZPat.t, HTyp.t, Ctx.t)) =>
    switch (zp) {
    | Z(prefix, suffix) =>
      switch (a) {
      | Delete =>
        switch (prefix) {
        | [] => None
        | [t, ...prefix] =>
          let (tiles, n) =
            HPat.delete_tile_and_fix_empty_holes(prefix, t, suffix);
          let ((new_prefix, new_suffix), ty, ctx) =
            syn_fix_and_split(ctx, n, tiles);
          Some((Z(new_prefix, new_suffix), ty, ctx));
        }
      | Construct(s) =>
        switch (tile_of_shape(s)) {
        | None => None
        | Some(tile) =>
          let (tiles, n) =
            HPat.insert_tile_and_fix_empty_holes(prefix, tile, suffix);
          let ((new_prefix, new_suffix), ty, ctx) =
            syn_fix_and_split(ctx, n, tiles);
          Some((Z(new_prefix, new_suffix), ty, ctx));
        }
      }

    | ParenZ(zbody) =>
      syn_perform(ctx, a, zbody)
      |> Option.map(((zbody, ty, ctx)) => (ZPat.ParenZ(zbody), ty, ctx))

    | AnnZ(_, subj, zann) =>
      Typ.perform(a, zann)
      |> Option.map(zann => {
           let ann = ZTyp.erase(zann);
           let (subj, ctx) = Statics.Pat.ana_fix_holes(ctx, subj, ann);
           (ZPat.AnnZ(NotInHole, subj, zann), ann, ctx);
         })
    }
  and ana_perform =
      (ctx: Ctx.t, a: t, zp: ZPat.t, ty: HTyp.t): option((ZPat.t, Ctx.t)) =>
    switch (zp) {
    | Z(prefix, suffix) =>
      switch (a) {
      | Delete =>
        switch (prefix) {
        | [] => None
        | [t, ...prefix] =>
          let (tiles, n) =
            HPat.delete_tile_and_fix_empty_holes(prefix, t, suffix);
          let ((new_prefix, new_suffix), ctx) =
            ana_fix_and_split(ctx, n, tiles, ty);
          Some((Z(new_prefix, new_suffix), ctx));
        }
      | Construct(s) =>
        switch (tile_of_shape(s)) {
        | None => None
        | Some(tile) =>
          let (tiles, n) =
            HPat.insert_tile_and_fix_empty_holes(prefix, tile, suffix);
          let ((new_prefix, new_suffix), ctx) =
            ana_fix_and_split(ctx, n, tiles, ty);
          Some((Z(new_prefix, new_suffix), ctx));
        }
      }

    | ParenZ(zbody) =>
      ana_perform(ctx, a, zbody, ty)
      |> Option.map(((zbody, ctx)) => (ZPat.ParenZ(zbody), ctx))

    | AnnZ(_, subj, zann) =>
      Typ.perform(a, zann)
      |> Option.map(zann => {
           let ann = ZTyp.erase(zann);
           let (subj, ctx) = Statics.Pat.ana_fix_holes(ctx, subj, ann);
           let status =
             HTyp.consistent(ty, ann) ? HoleStatus.NotInHole : InHole;
           (ZPat.AnnZ(status, subj, zann), ctx);
         })
    };
};

module Exp = {
  let tile_of_shape: tile_shape => option(HExp.Tile.t) =
    fun
    | Ann
    | Arrow => None
    | Num(n) => Some(Num(NotInHole, n))
    | Var(x) => Some(Var(NotInHole, x))
    | Paren => Some(Paren(HExp.OperandHole))
    | Lam => Some(Lam(NotInHole, HPat.OperandHole))
    | Ap => Some(Ap(NotInHole, HExp.OperandHole))
    | Plus => Some(Plus(NotInHole));

  let syn_fix_and_split = (ctx, n, tiles) => {
    let (new_e, ty) = Statics.Exp.syn_fix_holes(ctx, HExp.parse(tiles));
    (ListUtil.split_n(n, HExp.Tile.unparse(new_e)), ty);
  };
  let ana_fix_and_split = (ctx, n, tiles, ty) => {
    let new_e = Statics.Exp.ana_fix_holes(ctx, HExp.parse(tiles), ty);
    ListUtil.split_n(n, HExp.Tile.unparse(new_e));
  };

  let rec syn_perform =
          (ctx: Ctx.t, a: t, ze: ZExp.t): option((ZExp.t, HTyp.t)) =>
    switch (ze) {
    | Z(prefix, suffix) =>
      switch (a) {
      // z + ( x + y )|
      // --> z + x + y |
      | Delete =>
        switch (prefix) {
        | [] => None
        | [t, ...prefix] =>
          let (tiles, n) =
            HExp.delete_tile_and_fix_empty_holes(prefix, t, suffix);
          let ((new_prefix, new_suffix), ty) =
            syn_fix_and_split(ctx, n, tiles);
          Some((Z(new_prefix, new_suffix), ty));
        }
      | Construct(s) =>
        switch (tile_of_shape(s)) {
        | None => None
        | Some(tile) =>
          let (tiles, n) =
            HExp.insert_tile_and_fix_empty_holes(prefix, tile, suffix);
          let ((new_prefix, new_suffix), ty) =
            syn_fix_and_split(ctx, n, tiles);
          Some((Z(new_prefix, new_suffix), ty));
        }
      }

    | ParenZ(zbody) =>
      syn_perform(ctx, a, zbody)
      |> Option.map(((zbody, ty)) => (ZExp.ParenZ(zbody), ty))

    | LamZ(_, zp, body) =>
      switch (Pat.syn_perform(ctx, a, zp)) {
      | None => None
      | Some((zp, ty1, ctx)) =>
        let (body, ty2) = Statics.Exp.syn_fix_holes(ctx, body);
        Some((LamZ(NotInHole, zp, body), HTyp.Arrow(ty1, ty2)));
      }

    | ApZ(_, fn, zarg) =>
      switch (Statics.Exp.syn(ctx, fn)) {
      | None => None
      | Some(fn_ty) =>
        switch (HTyp.matched_arrow(fn_ty)) {
        | None => None
        | Some((ty1, ty2)) =>
          ana_perform(ctx, a, zarg, ty1)
          |> Option.map(zarg => (ZExp.ApZ(NotInHole, fn, zarg), ty2))
        }
      }
    }
  and ana_perform =
      (ctx: Ctx.t, a: t, ze: ZExp.t, ty: HTyp.t): option(ZExp.t) =>
    switch (ze) {
    | Z(prefix, suffix) =>
      switch (a) {
      | Delete =>
        switch (prefix) {
        | [] => None
        | [t, ...prefix] =>
          let (tiles, n) =
            HExp.delete_tile_and_fix_empty_holes(prefix, t, suffix);
          let (new_prefix, new_suffix) =
            ana_fix_and_split(ctx, n, tiles, ty);
          Some(Z(new_prefix, new_suffix));
        }
      | Construct(s) =>
        switch (tile_of_shape(s)) {
        | None => None
        | Some(tile) =>
          let (tiles, n) =
            HExp.insert_tile_and_fix_empty_holes(prefix, tile, suffix);
          let (new_prefix, new_suffix) =
            ana_fix_and_split(ctx, n, tiles, ty);
          Some(Z(new_prefix, new_suffix));
        }
      }

    | ParenZ(zbody) =>
      ana_perform(ctx, a, zbody, ty)
      |> Option.map(zbody => ZExp.ParenZ(zbody))

    | LamZ(_, zp, body) =>
      switch (HTyp.matched_arrow(ty)) {
      | None =>
        syn_perform(ctx, a, ze)
        |> Option.map(((ze, _)) => ZExp.set_hole_status(InHole, ze))
      | Some((ty1, ty2)) =>
        Pat.ana_perform(ctx, a, zp, ty1)
        |> Option.map(((zp, ctx)) => {
             let body = Statics.Exp.ana_fix_holes(ctx, body, ty2);
             ZExp.LamZ(NotInHole, zp, body);
           })
      }

    | ApZ(_) =>
      syn_perform(ctx, a, ze)
      |> Option.map(((ze, ty')) =>
           HTyp.consistent(ty, ty') ? ze : ZExp.set_hole_status(InHole, ze)
         )
    };
};
