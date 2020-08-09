module Term = {
  type t =
    | EHole
    | NHole(t)
    | Num(int)
    | Var(Var.t)
    | Paren(t)
    | If(t, t, t)
    | Let(HPat.Term.t, t, t)
    | Ann(t, HTyp.Term.t)
    | Plus(t, t)
    | Times(t, t)
    | Eq(t, t)
    | OHole(t, t);
};

module Tile = {
  type term = Term.t;
  type t =
    | EHole
    | Num(int)
    | Var(Var.t)
    | Paren(term)
    | If(term, term)
    | Let(HPat.Term.t, term)
    | Ann(HTyp.Term.t)
    | OHole
    | Plus
    | Times
    | Eq;

  let operand_hole = () => EHole;
  let operator_hole = () => OHole;

  let shape: t => TileShape.t(term) =
    fun
    | EHole => Operand(Term.EHole)
    | Num(n) => Operand(Term.Num(n))
    | Var(x) => Operand(Term.Var(x))
    | Paren(body) => Operand(Term.Paren(body))
    | If(cond, then_) => PreOp(else_ => Term.If(cond, then_, else_), 10)
    | Let(p, def) => PreOp(body => Term.Let(p, def, body), 10)
    | Ann(ann) => PostOp(subj => Term.Ann(subj, ann), 9)
    | Plus => BinOp((e1, e2) => Term.Plus(e1, e2), 3, Left)
    | Times => BinOp((e1, e2) => Term.Times(e1, e2), 2, Left)
    | Eq => BinOp((e1, e2) => Term.Eq(e1, e2), 5, Left)
    | OHole => BinOp((e1, e2) => Term.Eq(e1, e2), 1, Left);
};

let parse = TileParser.parse((module Tile));
let rec unparse: Term.t => list(Tile.t) =
  fun
  | EHole => [EHole]
  | NHole(e) =>
    // TODO add err status to tiles
    unparse(e)
  | Num(n) => [Num(n)]
  | Var(x) => [Var(x)]
  | Paren(body) => [Paren(body)]
  | If(cond, then_, else_) => [If(cond, then_), ...unparse(else_)]
  | Let(p, def, body) => [Let(p, def), ...unparse(body)]
  | Ann(subj, ann) => unparse(subj) @ [Ann(ann)]
  | Plus(e1, e2) => unparse(e1) @ [Plus, ...unparse(e2)]
  | Times(e1, e2) => unparse(e1) @ [Times, ...unparse(e2)]
  | Eq(e1, e2) => unparse(e1) @ [Eq, ...unparse(e2)]
  | OHole(e1, e2) => unparse(e1) @ [OHole, ...unparse(e2)];