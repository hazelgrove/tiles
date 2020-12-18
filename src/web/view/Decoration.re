open Sexplib.Std;
open Virtual_dom.Vdom;

module Sort = Core.Sort;
module Direction = Core.Direction;

let hole_radii = (~font_metrics: FontMetrics.t) => {
  let r = 3.5;
  (r /. font_metrics.col_width, r /. font_metrics.row_height);
};

let tip = 0.3;
let child_border_thickness = 0.105;

module Diag = {
  let tr_bl = (~child_border: option([ | `North | `South])=?, ()) =>
    SvgUtil.Path.(
      switch (child_border) {
      | None =>
        let diag = L_({dx: Float.neg(tip), dy: 0.5});
        [diag];
      | Some(d) =>
        let t = child_border_thickness /. 0.5;
        let dx = (1. -. t) *. tip;
        let diag = L_({dx: Float.neg(dx), dy: (1. -. t) *. 0.5});
        let border = H_({dx: Float.neg(1. -. dx)});
        switch (d) {
        | `North => [border, diag]
        | `South => [diag, border]
        };
      }
    );
  let bl_tr = (~child_border: option([ | `North | `South])=?, ()) =>
    SvgUtil.Path.reverse(tr_bl(~child_border?, ()));

  let tl_br = (~child_border: option([ | `North | `South])=?, ()) =>
    SvgUtil.Path.(
      switch (child_border) {
      | None =>
        let diag = L_({dx: tip, dy: 0.5});
        [diag];
      | Some(d) =>
        let t = child_border_thickness /. 0.5;
        let dx = (1. -. t) *. tip;
        let diag = L_({dx, dy: (1. -. t) *. 0.5});
        let border = H_({dx: 1. -. dx});
        switch (d) {
        | `North => [border, diag]
        | `South => [diag, border]
        };
      }
    );
  let br_tl = (~child_border: option([ | `North | `South])=?, ()) =>
    SvgUtil.Path.reverse(tl_br(~child_border?, ()));
};

module ErrHole = {
  type profile = {
    expanded: bool,
    len: int,
  };

  let view = ({expanded, len}: profile): list(Node.t) => {
    open SvgUtil.Path;
    open Diag;
    let len = Float.of_int(len);
    let tl_br = expanded ? tl_br() : tl_br(~child_border=`North, ());
    let tr_bl = expanded ? tr_bl() : tr_bl(~child_border=`South, ());
    let br_tl = expanded ? br_tl() : br_tl(~child_border=`South, ());
    let bl_tr = expanded ? bl_tr() : bl_tr(~child_border=`North, ());
    let path =
      List.concat([
        [
          M({x: 0., y: expanded ? 0. : child_border_thickness}),
          H_({dx: len}),
        ],
        tl_br,
        tr_bl,
        [H_({dx: Float.neg(len)})],
        br_tl,
        bl_tr,
        [Z],
      ]);
    let attrs =
      Attr.[
        classes(["err-hole-path"]),
        create("vector-effect", "non-scaling-stroke"),
      ];
    [view(~attrs, path)];
  };
};

module EmptyHole = {
  let inset_shadow_filter =
    Node.create_svg(
      "filter",
      [Attr.id("empty-hole-inset-shadow")],
      [
        Node.create_svg(
          "feOffset",
          Attr.[
            create("in", "SourceAlpha"),
            create("dx", "0.1"),
            create("dy", "0.04"),
            create("result", "offset-alpha"),
          ],
          [],
        ),
        Node.create_svg(
          "feFlood",
          Attr.[
            classes(["empty-hole-inset-shadow-flood"]),
            create("flood-opacity", "1"),
            create("result", "color"),
          ],
          [],
        ),
        Node.create_svg(
          "feComposite",
          Attr.[
            create("operator", "out"),
            create("in", "SourceAlpha"),
            create("in2", "offset-alpha"),
            create("result", "shadow-shape"),
          ],
          [],
        ),
        Node.create_svg(
          "feComposite",
          Attr.[
            create("operator", "in"),
            create("in", "color"),
            create("in2", "shadow-shape"),
            create("result", "drop-shadow"),
          ],
          [],
        ),
        Node.create_svg(
          "feMerge",
          [],
          [
            Node.create_svg(
              "feMergeNode",
              [Attr.create("in", "SourceGraphic")],
              [],
            ),
            Node.create_svg(
              "feMergeNode",
              [Attr.create("in", "drop-shadow")],
              [],
            ),
          ],
        ),
      ],
    );

  let thin_inset_shadow_filter =
    Node.create_svg(
      "filter",
      [Attr.id("empty-hole-thin-inset-shadow")],
      [
        Node.create_svg(
          "feOffset",
          Attr.[
            create("in", "SourceAlpha"),
            create("dx", "0.06"),
            create("dy", "0.024"),
            create("result", "offset-alpha"),
          ],
          [],
        ),
        Node.create_svg(
          "feFlood",
          Attr.[
            classes(["empty-hole-inset-shadow-flood"]),
            create("flood-opacity", "1"),
            create("result", "color"),
          ],
          [],
        ),
        Node.create_svg(
          "feComposite",
          Attr.[
            create("operator", "out"),
            create("in", "SourceAlpha"),
            create("in2", "offset-alpha"),
            create("result", "shadow-shape"),
          ],
          [],
        ),
        Node.create_svg(
          "feComposite",
          Attr.[
            create("operator", "in"),
            create("in", "color"),
            create("in2", "shadow-shape"),
            create("result", "drop-shadow"),
          ],
          [],
        ),
        Node.create_svg(
          "feMerge",
          [],
          [
            Node.create_svg(
              "feMergeNode",
              [Attr.create("in", "SourceGraphic")],
              [],
            ),
            Node.create_svg(
              "feMergeNode",
              [Attr.create("in", "drop-shadow")],
              [],
            ),
          ],
        ),
      ],
    );

  let view =
      (
        ~offset=0,
        ~inset: option([ | `Thick | `Thin]),
        ~font_metrics: FontMetrics.t,
        (),
      )
      : list(Node.t) => {
    // necessary because of AttrUtil shadowing below
    let o = offset;
    let (r_x, r_y) = hole_radii(~font_metrics);
    [
      switch (inset) {
      | Some(`Thick) => inset_shadow_filter
      | _ => thin_inset_shadow_filter
      },
      Node.create_svg(
        "ellipse",
        AttrUtil.[
          cx(Float.of_int(o) +. 0.5),
          cy(0.5),
          rx(r_x),
          ry(r_y),
          vector_effect("non-scaling-stroke"),
          stroke_width(Option.is_some(inset) ? 0.3 : 0.75),
          filter(
            switch (inset) {
            | None => "none"
            | Some(`Thin) => "url(#empty-hole-thin-inset-shadow)"
            | Some(`Thick) => "url(#empty-hole-inset-shadow)"
            },
          ),
          Attr.classes(["empty-hole-path"]),
        ],
        [],
      ),
    ];
  };
};

module OpenChild = {
  type profile = {
    sort: Sort.t,
    side: Direction.t,
    len: int,
  };

  let view = ({sort, side, len}: profile): list(Node.t) => {
    open SvgUtil.Path;
    open Diag;
    let len = Float.of_int(len + 1);
    /* old raised tile path
       let path =
         List.concat([
           [M({x: 0., y: 0.}), H_({dx: len})],
           tl_br(),
           tr_bl(),
           [H_({dx: Float.neg(len)})],
           br_tl(),
           bl_tr(),
           [Z],
         ]);
       */
    let (gradient_id, gradient) = {
      let id =
        switch (side) {
        | Left => "open-child-gradient-left"
        | Right => "open-child-gradient-right"
        };
      let (x1, x2) =
        switch (side) {
        | Left => ("0", Printf.sprintf("%f", len))
        | Right => (Printf.sprintf("%f", len), "0")
        };
      let color =
        switch (sort) {
        | Typ => "var(--typ-shadow-color)"
        | Pat => "var(--pat-shadow-color)"
        | Exp => "var(--exp-shadow-color)"
        };
      let gradient =
        Node.create_svg(
          "linearGradient",
          [
            Attr.id(id),
            Attr.create("gradientUnits", "userSpaceOnUse"),
            Attr.create("x1", x1),
            Attr.create("x2", x2),
          ],
          [
            Node.create_svg(
              "stop",
              [Attr.create("offset", "0%"), AttrUtil.stop_color(color)],
              [],
            ),
            Node.create_svg(
              "stop",
              [
                Attr.create(
                  "offset",
                  Printf.sprintf("%f%%", 100. *. (len -. 1.25) /. len),
                ),
                AttrUtil.stop_color(color),
              ],
              [],
            ),
            Node.create_svg(
              "stop",
              [
                Attr.create(
                  "offset",
                  Printf.sprintf("%f%%", 100. *. (len -. 0.6) /. len),
                ),
                AttrUtil.stop_color(color),
                AttrUtil.stop_opacity("0"),
              ],
              [],
            ),
          ],
        );
      (id, gradient);
    };
    let path =
      switch (side) {
      | Left =>
        List.concat([
          [M({x: len, y: 0.}), H_({dx: Float.neg(len)})],
          // [M({x: 0., y: 0.})],
          tr_bl(),
          tl_br(),
          [H_({dx: len})],
          [M_({dx: 0., dy: 0.02}), H_({dx: Float.neg(len)})],
        ])
      | Right =>
        List.concat([
          [M({x: 0., y: 0.}), H_({dx: len})],
          // [M({x: len, y: 0.})],
          tl_br(),
          tr_bl(),
          [H_({dx: Float.neg(len)})],
          [M_({dx: 0., dy: 0.02}), H_({dx: len +. 0.03})],
          bl_tr(),
          br_tl(),
        ])
      };
    let attrs =
      Attr.[
        classes(["open-child-path", Sort.to_string(sort)]),
        create("vector-effect", "non-scaling-stroke"),
        create("stroke", Printf.sprintf("url(#%s)", gradient_id)),
      ];
    [gradient, view(~attrs, path)];
  };
};

module Tile = {
  [@deriving sexp]
  type style =
    | Highlighted
    | Unhighlighted(bool);

  [@deriving sexp]
  type profile = {
    shape: Core.Tile.t(bool, unit, unit, bool),
    len: int,
    open_children: list((int, int)),
    closed_children: list((int, int)),
    empty_holes: list(int),
    sort: Sort.t,
    style,
  };

  let open_child_paths =
      (~sort: Sort.t, open_children: list((int, int))): list(Node.t) => {
    open SvgUtil.Path;
    let color =
      switch (sort) {
      | Typ => "var(--typ-shadow-color)"
      | Pat => "var(--pat-shadow-color)"
      | Exp => "var(--exp-shadow-color)"
      };
    let gradient = (id, start, len) =>
      Node.create_svg(
        "linearGradient",
        [
          Attr.id(id),
          Attr.create("gradientUnits", "userSpaceOnUse"),
          Attr.create("x1", string_of_int(start)),
          // TODO unify with space
          Attr.create("x2", string_of_int(start + len + 2)),
        ],
        NodeUtil.[
          stop(
            AttrUtil.[
              // TODO unify with space
              offset(0.6 /. Float.of_int(len + 2)),
              stop_color(color),
              stop_opacity("0"),
            ],
          ),
          stop(
            AttrUtil.[
              offset(1.25 /. Float.of_int(len + 2)),
              stop_color(color),
            ],
          ),
          stop(
            AttrUtil.[
              offset(
                (Float.of_int(len + 2) -. 1.25) /. Float.of_int(len + 2),
              ),
              stop_color(color),
            ],
          ),
          stop(
            AttrUtil.[
              offset(
                (Float.of_int(len + 2) -. 0.6) /. Float.of_int(len + 2),
              ),
              stop_color(color),
              stop_opacity("0"),
            ],
          ),
        ],
      );
    open_children
    |> List.map(((start, len)) => {
         let gradient_id =
           Printf.sprintf("bidelimited-open-child-gradient-%d", start);
         [
           gradient(gradient_id, start, len),
           view(
             ~attrs=
               Attr.[
                 classes([
                   Sort.to_string(sort),
                   "bidelimited-open-child-path",
                 ]),
                 AttrUtil.vector_effect("non-scaling-stroke"),
                 create("stroke", Printf.sprintf("url(#%s)", gradient_id)),
               ],
             [
               M({x: Float.of_int(start), y: 0.}),
               H_({dx: Float.of_int(len + 2)}),
             ],
           ),
         ];
       })
    |> List.flatten;
  };

  let contour_path = (~attrs: list(Attr.t), profile: profile): Node.t => {
    open SvgUtil.Path;
    open Diag;
    let closed_child_paths =
      profile.closed_children
      |> List.map(((start, len)) =>
           List.concat([
             [M({x: Float.of_int(start + 1), y: child_border_thickness})],
             tr_bl(~child_border=`North, ()),
             tl_br(~child_border=`South, ()),
             [H_({dx: Float.of_int(len)})],
             bl_tr(~child_border=`South, ()),
             br_tl(~child_border=`North, ()),
             [Z],
           ])
         );
    let outer_path = {
      let (left_tip, right_tip) =
        switch (profile.shape) {
        | Operand(_) => (br_tl() @ bl_tr(), tl_br() @ tr_bl())
        | PreOp () => (
            br_tl() @ bl_tr(),
            [H_({dx: tip}), ...tr_bl()]
            @ tl_br()
            @ [H_({dx: Float.neg(tip)})],
          )
        | PostOp () => (
            [H_({dx: Float.neg(tip)}), ...bl_tr()]
            @ br_tl()
            @ [H_({dx: tip})],
            tl_br() @ tr_bl(),
          )
        | BinOp(_) => (
            [H_({dx: Float.neg(tip)}), ...bl_tr()]
            @ br_tl()
            @ [H_({dx: tip})],
            [H_({dx: tip}), ...tr_bl()]
            @ tl_br()
            @ [H_({dx: Float.neg(tip)})],
          )
        };
      let open_child_contours =
        profile.open_children
        |> List.map(((start, len)) =>
             List.concat([
               [H({x: Float.of_int(start) +. tip})],
               tr_bl(),
               tl_br(~child_border=`South, ()),
               [H_({dx: Float.of_int(len)})],
               bl_tr(~child_border=`South, ()),
               br_tl(),
             ])
           )
        |> List.concat;
      List.concat([
        [M({x: 0., y: 1.}), ...left_tip],
        open_child_contours,
        [H({x: Float.of_int(profile.len)}), ...right_tip],
        [Z],
      ]);
    };
    let clss =
      List.concat([
        ["tile-path", Sort.to_string(profile.sort)],
        switch (profile.style) {
        | Highlighted => ["highlighted", "with-shadow"]
        | Unhighlighted(_) => []
        },
      ]);
    SvgUtil.Path.view(
      ~attrs=
        Attr.[
          classes(clss),
          create("vector-effect", "non-scaling-stroke"),
          ...attrs,
        ],
      outer_path @ List.concat(closed_child_paths),
    );
  };

  let shadow_filter = (~sort: Sort.t) =>
    Node.create_svg(
      "filter",
      [Attr.id("outer-drop-shadow")],
      [
        Node.create_svg(
          "feDropShadow",
          [
            Attr.classes(["tile-drop-shadow", Sort.to_string(sort)]),
            Attr.create("dx", "0.1"),
            Attr.create("dy", "0.04"),
            Attr.create("stdDeviation", "0"),
          ],
          [],
        ),
      ],
    );

  let thin_shadow_filter = (~sort: Sort.t) =>
    Node.create_svg(
      "filter",
      [Attr.id("thin-outer-drop-shadow")],
      [
        Node.create_svg(
          "feDropShadow",
          [
            Attr.classes(["tile-drop-shadow", Sort.to_string(sort)]),
            Attr.create("dx", "0.06"),
            Attr.create("dy", "0.024"),
            Attr.create("stdDeviation", "0"),
          ],
          [],
        ),
      ],
    );

  let view =
      (
        ~attrs: list(Attr.t)=[],
        ~font_metrics: FontMetrics.t,
        profile: profile,
      )
      : list(Node.t) => {
    let empty_holes =
      switch (profile.style) {
      | Highlighted =>
        switch (profile.shape) {
        | Operand(true)
        | BinOp(true) =>
          EmptyHole.view(~offset=0, ~font_metrics, ~inset=Some(`Thick), ())
        | _ => []
        }
      | Unhighlighted(show_children) =>
        show_children
          ? []
          : profile.empty_holes
            |> List.map(offset =>
                 EmptyHole.view(
                   ~offset,
                   ~font_metrics,
                   ~inset=Some(`Thin),
                   (),
                 )
               )
            |> List.flatten
      };
    let profile =
      switch (profile.style) {
      | Unhighlighted(false) => {
          ...profile,
          open_children: [],
          closed_children: [],
        }
      | _ => profile
      };
    open_child_paths(~sort=profile.sort, profile.open_children)
    @ [
      shadow_filter(~sort=profile.sort),
      thin_shadow_filter(~sort=profile.sort),
      contour_path(~attrs, profile),
      ...empty_holes,
    ];
  };
};

module Caret = {
  let action_type = txt =>
    Node.div([Attr.classes(["action-type"])], [Node.text(txt)]);

  let construct_shape = txt =>
    Node.div([Attr.classes(["construct-shape"])], [Node.text(txt)]);

  let key = txt => Node.div([Attr.classes(["key"])], [Node.text(txt)]);

  let keys_container = Node.div([Attr.classes(["keys"])]);
  let keys = ks => keys_container(List.map(key, ks));

  let move_row = [
    keys([Unicode.left_arrow, Unicode.right_arrow]),
    action_type("Move"),
  ];

  let select_row = [
    keys_container([
      key("Shift"),
      Node.div([], [Node.text("+")]),
      key(Unicode.left_arrow),
      key(Unicode.right_arrow),
    ]),
    action_type("Select"),
  ];

  let delete_row = [
    keys(["Backspace", "Del"]),
    action_type("Delete/Restructure"),
  ];

  let buffer_cell = Node.div([], []);
  let buffer_row = [buffer_cell, buffer_cell];

  let construct_rows =
    List.concat(
      Util.ListUtil.join(
        buffer_row,
        [
          [buffer_cell, action_type("Construct")],
          [
            keys_container([
              key("0"),
              Node.div([], [Node.text("-")]),
              key("9"),
            ]),
            construct_shape("single-digit num"),
          ],
          [
            keys_container([
              key("a"),
              Node.div([], [Node.text("-")]),
              key("z"),
            ]),
            construct_shape("single-char var"),
          ],
          [keys(["+"]), construct_shape("plus")],
          [keys(["("]), construct_shape("parentheses")],
          [keys(["\\"]), construct_shape("lambda")],
          [keys(["="]), construct_shape("let expression")],
        ],
      ),
    );

  let view = (~font_metrics: FontMetrics.t, offset: int, _) =>
    Node.div(
      [
        Attr.id("caret"),
        Attr.create(
          "style",
          Printf.sprintf(
            "top: 0; left: %fpx",
            (Float.of_int(offset) +. 0.5) *. font_metrics.col_width,
          ),
        ),
      ],
      [
        Node.div(
          [Attr.id("action-table")],
          List.concat([
            move_row,
            buffer_row,
            select_row,
            buffer_row,
            delete_row,
            buffer_row,
            construct_rows,
          ]),
        ),
      ],
    );
};

let container =
    (
      ~font_metrics: FontMetrics.t,
      ~origin: int=0,
      ~length: int,
      ~cls: string,
      svgs: list(Node.t),
    )
    : Node.t => {
  let buffered_height = 2;
  let buffered_width = length + 1;

  let buffered_height_px =
    Float.of_int(buffered_height) *. font_metrics.row_height;
  let buffered_width_px =
    Float.of_int(buffered_width) *. font_metrics.col_width;

  let container_origin_x =
    (Float.of_int(origin) -. 0.5) *. font_metrics.col_width;
  let container_origin_y = (-0.5) *. font_metrics.row_height;

  Node.div(
    [
      Attr.classes([
        "decoration-container",
        Printf.sprintf("%s-container", cls),
      ]),
      Attr.create(
        "style",
        Printf.sprintf(
          "top: %fpx; left: %fpx;",
          container_origin_y,
          container_origin_x,
        ),
      ),
    ],
    [
      Node.create_svg(
        "svg",
        [
          Attr.classes([cls]),
          Attr.create(
            "viewBox",
            Printf.sprintf(
              "-0.5 -0.5 %d %d",
              buffered_width,
              buffered_height,
            ),
          ),
          Attr.create("width", Printf.sprintf("%fpx", buffered_width_px)),
          Attr.create("height", Printf.sprintf("%fpx", buffered_height_px)),
          Attr.create("preserveAspectRatio", "none"),
        ],
        svgs,
      ),
    ],
  );
};
