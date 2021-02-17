open Util.OptUtil.Syntax;

type t =
  | Uni(unidelimited)
  | Bi(bidelimited)
and unidelimited =
  | Pre_r(Term_exp.pre, t)
  | Post_l(t, Term_exp.post)
  | Bin_l(t, Term_exp.bin, Term_exp.t)
  | Bin_r(Term_exp.t, Term_exp.bin, t)
and bidelimited =
  | Root
  // (_) == Paren_body(Bi(None))
  | Paren_body(t)
  // ( let x = _ in x )
  // == Let_def(Var(x), Bi(Some(Paren_body(Bi(None)))), Var(x))
  | Let_def(Term_pat.t, t, Term_exp.t)
  | Ap_arg(Term_exp.t, t);

let root = Root;

let rec append_exp = (frame: t, frame_exp: bidelimited): option(t) =>
  switch (frame) {
  | Uni(Pre_r(pre, frame)) =>
    let+ frame = append_exp(frame, frame_exp);
    Uni(Pre_r(pre, frame));
  | Uni(Post_l(frame, post)) =>
    let+ frame = append_exp(frame, frame_exp);
    Uni(Post_l(frame, post));
  | Uni(Bin_l(frame, bin, r)) =>
    let+ frame = append_exp(frame, frame_exp);
    Uni(Bin_l(frame, bin, r));
  | Uni(Bin_r(l, bin, frame)) =>
    let+ frame = append_exp(frame, frame_exp);
    Uni(Bin_r(l, bin, frame));
  | Bi(bidelimited) =>
    let+ bidelimited = bidelimited_append_exp(bidelimited, frame_exp);
    Bi(bidelimited);
  }
and bidelimited_append_exp = (frame: bidelimited, frame_exp) =>
  switch (frame) {
  | Root => Some(frame_exp)
  | Paren_body(frame) =>
    let+ frame = append_exp(frame, frame_exp);
    Paren_body(frame);
  | Let_def(p, frame, body) =>
    let+ frame = append_exp(frame, frame_exp);
    Let_def(p, frame, body);
  | Ap_arg(_) => failwith("ap todo")
  };
