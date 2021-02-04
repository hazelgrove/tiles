[@deriving sexp]
type t('op, 'pre, 'post, 'bin) =
  | Op('op)
  | Pre('pre)
  | Post('post)
  | Bin('bin);

let get_operand: t('op, _, _, _) => 'op =
  fun
  | Op(op) => op
  | _ => raise(Invalid_argument("Tile.get_operand"));
let get_preop: t(_, 'pre, _, _) => 'pre =
  fun
  | Pre(pre) => pre
  | _ => raise(Invalid_argument("Tile.get_preop"));
let get_postop: t(_, _, 'post, _) => 'post =
  fun
  | Post(post) => post
  | _ => raise(Invalid_argument("Tile.get_postop"));
let get_binop: t(_, _, _, 'bin) => 'bin =
  fun
  | Bin(bin) => bin
  | _ => raise(Invalid_argument("Tile.get_binop"));

let is_operand =
  fun
  | Op(_) => true
  | _ => false;
let is_binop =
  fun
  | Bin(_) => true
  | _ => false;

let get =
    (
      get_operand: 'op => 'a,
      get_preop: 'pre => 'a,
      get_postop: 'post => 'a,
      get_binop: 'bin => 'a,
    )
    : (t('op, 'pre, 'post, 'bin) => 'a) =>
  fun
  | Op(op) => get_operand(op)
  | Pre(pre) => get_preop(pre)
  | Post(post) => get_postop(post)
  | Bin(bin) => get_binop(bin);

let flip: t('op, 'pre, 'post, 'bin) => t('op, 'post, 'pre, 'bin) =
  fun
  | (Op(_) | Bin(_)) as t => t
  | Pre(pre) => Post(pre)
  | Post(post) => Pre(post);

let rev =
    (ts: list(t('op, 'pre, 'post, 'bin))): list(t('op, 'post, 'pre, 'bin)) =>
  List.rev_map(flip, ts);

/*
 module type S = {
   let sort: Sort.t;

   type op;
   type pre;
   type post;
   type bin;
   type nonrec t = t(op, pre, post, bin);
   type s = list(t);

   let mk_op_hole: unit => op;
   let mk_bin_hole: unit => bin;

   let is_op_hole: op => bool;
   let is_bin_hole: bin => bool;

   let precedence: t => int;
   let associativity: Util.IntMap.t(Associativity.t);

   type closed_descendant;
   type descendant = Descendant.t(s, closed_descendant);
   let get_open_children: t => list(s);
 };
 */
