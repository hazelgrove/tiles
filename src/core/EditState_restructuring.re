type t =
  | Typ(Zipper_typ.restructuring)
  | Pat(Zipper_pat.restructuring)
  | Exp(Zipper_exp.restructuring);

let of_pointing = selections =>
  fun
  | EditState_pointing.Typ((pointing, frame)) =>
    Typ((Subject.restructuring_of_pointing(selections, pointing), frame))
  | Pat((pointing, frame)) =>
    Pat((Subject.restructuring_of_pointing(selections, pointing), frame))
  | Exp((pointing, frame)) =>
    Exp((Subject.restructuring_of_pointing(selections, pointing), frame));