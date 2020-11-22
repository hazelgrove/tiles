module Mode = {
  type t =
    | Normal(ZPath.t)
    | Selecting(ZPath.anchored_selection)
    | Restructuring(ZPath.ordered_selection, ZPath.t);

  let cons = two_step =>
    fun
    | Normal(focus) => Normal(ZPath.cons(two_step, focus))
    | Selecting(selection) =>
      Selecting(ZPath.cons_anchored_selection(two_step, selection))
    | Restructuring(selection, focus) =>
      Restructuring(
        ZPath.cons_ordered_selection(two_step, selection),
        ZPath.cons(two_step, focus),
      );

  let get_focus =
    fun
    | Normal(focus)
    | Selecting({focus, _})
    | Restructuring(_, focus) => focus;

  let put_focus = focus =>
    fun
    | Normal(_) => Normal(focus)
    | Selecting({anchor, _}) => Selecting({anchor, focus})
    | Restructuring(selection, _) => Restructuring(selection, focus);

  let update_anchors = (f: ZPath.t => ZPath.t) =>
    fun
    | Normal(_) as mode => mode
    | Selecting({anchor, focus}) => Selecting({anchor: f(anchor), focus})
    | Restructuring((l, r), focus) => Restructuring((f(l), f(r)), focus);
};

module Zipper = {
  type t = [ | `Exp(ZExp.zipper) | `Pat(ZPat.zipper) | `Typ(ZTyp.zipper)];
  type did_it_zip = option((ZPath.two_step, t));

  let unzip = (two_step, zipper: t) =>
    switch (zipper) {
    | `Typ(zipper) => (ZPath.Typ.unzip(two_step, zipper) :> t)
    | `Pat(zipper) => (ZPath.Pat.unzip(two_step, zipper) :> t)
    | `Exp(zipper) => (ZPath.Exp.unzip(two_step, zipper) :> t)
    };
};

type t = (Mode.t, Zipper.t);

let zip_up = ((mode, zipper) as edit_state: t): t =>
  switch (zipper) {
  | `Typ(_, None)
  | `Pat(_, None)
  | `Exp(_, None) => edit_state
  | `Typ(ty, Some(ztile)) =>
    let (two_step, zipped) = ZPath.Typ.zip_ztile(ty, ztile);
    let mode = Mode.cons(two_step, mode);
    (mode, (zipped :> Zipper.t));
  | `Pat(p, Some(ztile)) =>
    let (two_step, zipped) = ZPath.Pat.zip_ztile(p, ztile);
    let mode = Mode.cons(two_step, mode);
    (mode, (zipped :> Zipper.t));
  | `Exp(e, Some(ztile)) =>
    let (two_step, zipped) = ZPath.Exp.zip_ztile(e, ztile);
    let mode = Mode.cons(two_step, mode);
    (mode, (zipped :> Zipper.t));
  };
