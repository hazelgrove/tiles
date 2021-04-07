open Sexplib.Std;

// ex: 1 + 2 _ 3 // Bin(Op(1), Plus, Bin(Op(2), BinHole, Op(3)))
[@deriving sexp]
type t = Term.t(op, pre, post, bin)
and op =
  // <_>
  | OpHole
  | Num(int)
  | Var(Var.t)
  | Paren(t)
and pre =
  // <_<
  | Lam(Term_pat.t)
  | Let(Term_pat.t, t)
and post =
  // >_>
  | Ap(t)
and bin =
  // >_<
  | Plus
  | BinHole
  | Prod
  | Cond(t);

let mk_op_hole = () => OpHole;
let mk_bin_hole = () => BinHole;

let is_op_hole =
  fun
  | OpHole => true
  | _ => false;
let is_bin_hole =
  fun
  | BinHole => true
  | _ => false;
