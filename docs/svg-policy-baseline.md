# SVG policy usability baseline

Generated: 2026-11-12T03:07:36.780Z

Input: `resources` (271 PAGX files).
Static export only (`--svg-no-animation`).

`bake` counts outputs containing an embedded PNG data URI; authored images also land in
this bucket, so treat it as an upper bound on effect baking.

| Policy | native | bake | reject |
|---|---:|---:|---:|
| vector-only | 241 | 4 | 26 |
| preserve-appearance | 241 | 21 | 9 |
| best-effort | 256 | 6 | 9 |

## Reject first causes

| Policy | causes |
|---|---|
| vector-only | unsupported-feature: 17, import/resource: 9 |
| preserve-appearance | import/resource: 9 |
| best-effort | import/resource: 9 |

Per-file results:

| File | vector-only | preserve-appearance | best-effort |
|---|---|---|---|
| `resources/apitest/api_consistency.pagx` | native | native | native |
| `resources/apitest/shadow_spread.pagx` | native | native | native |
| `resources/cli/bounds_layout.pagx` | native | native | native |
| `resources/cli/embed_font_file.pagx` | native | native | native |
| `resources/cli/embed_sample.pagx` | native | native | native |
| `resources/cli/embed_url_image.pagx` | native | native | native |
| `resources/cli/format_attr_order.pagx` | native | native | native |
| `resources/cli/format_comments.pagx` | native | native | native |
| `resources/cli/format_deep_nesting.pagx` | native | native | native |
| `resources/cli/format_messy_indent.pagx` | native | native | native |
| `resources/cli/format_preserve_newline.pagx` | native | native | native |
| `resources/cli/format_preserve_values.pagx` | native | native | native |
| `resources/cli/format_resources.pagx` | native | native | native |
| `resources/cli/import_node_basic.pagx` | reject (import/resource) | reject (import/resource) | reject (import/resource) |
| `resources/cli/import_node_none.pagx` | native | native | native |
| `resources/cli/import_resolve_dup_image_id.pagx` | reject (import/resource) | reject (import/resource) | reject (import/resource) |
| `resources/cli/import_resolve_multi_layer.pagx` | reject (import/resource) | reject (import/resource) | reject (import/resource) |
| `resources/cli/import_resolve_skew_siblings.pagx` | reject (import/resource) | reject (import/resource) | reject (import/resource) |
| `resources/cli/import_resolve_transformed_siblings.pagx` | reject (import/resource) | reject (import/resource) | reject (import/resource) |
| `resources/cli/layout_absolute.pagx` | native | native | native |
| `resources/cli/layout_clean.pagx` | native | native | native |
| `resources/cli/layout_clipped.pagx` | native | native | native |
| `resources/cli/layout_depth.pagx` | native | native | native |
| `resources/cli/layout_elements.pagx` | native | native | native |
| `resources/cli/layout_margins.pagx` | native | native | native |
| `resources/cli/layout_overlap.pagx` | native | native | native |
| `resources/cli/layout_zero_size.pagx` | native | native | native |
| `resources/cli/optimizer_all_rules.pagx` | native | native | native |
| `resources/cli/render_animated_timelines.pagx` | native | native | native |
| `resources/cli/render_background.pagx` | native | native | native |
| `resources/cli/render_basic.pagx` | native | native | native |
| `resources/cli/render_crop.pagx` | native | native | native |
| `resources/cli/render_gradient.pagx` | native | native | native |
| `resources/cli/render_layer_backdrop.pagx` | native | native | native |
| `resources/cli/render_layer_target.pagx` | native | native | native |
| `resources/cli/render_scale.pagx` | native | native | native |
| `resources/cli/render_text.pagx` | native | native | native |
| `resources/cli/resolve_conflict_children.pagx` | reject (import/resource) | reject (import/resource) | reject (import/resource) |
| `resources/cli/resolve_conflict_contents.pagx` | reject (import/resource) | reject (import/resource) | reject (import/resource) |
| `resources/cli/resolve_scaled_layer.pagx` | reject (import/resource) | reject (import/resource) | reject (import/resource) |
| `resources/cli/verify_background.pagx` | native | native | native |
| `resources/cli/verify_c10_complex_path.pagx` | native | native | native |
| `resources/cli/verify_c11_low_opacity.pagx` | native | native | native |
| `resources/cli/verify_c11_low_opacity_glass.pagx` | native | native | native |
| `resources/cli/verify_c13_simple_rect_mask.pagx` | native | native | native |
| `resources/cli/verify_c6_high_repeater.pagx` | native | native | native |
| `resources/cli/verify_c6_nested_repeater.pagx` | native | native | native |
| `resources/cli/verify_c7_high_blur.pagx` | native | native | native |
| `resources/cli/verify_c8_stroke_align.pagx` | native | native | native |
| `resources/cli/verify_c9_dashed_stroke.pagx` | native | native | native |
| `resources/cli/verify_centering_asymmetry_clean.pagx` | native | native | native |
| `resources/cli/verify_centering_asymmetry_content_measured.pagx` | native | native | native |
| `resources/cli/verify_centering_asymmetry_group.pagx` | native | native | native |
| `resources/cli/verify_centering_asymmetry_layer.pagx` | native | native | native |
| `resources/cli/verify_centering_asymmetry_nested.pagx` | native | native | native |
| `resources/cli/verify_child_exceeds_clipped.pagx` | native | native | native |
| `resources/cli/verify_child_exceeds_parent.pagx` | native | native | native |
| `resources/cli/verify_composition_blendmode_diff.pagx` | native | native | native |
| `resources/cli/verify_composition_constraint_diff.pagx` | native | native | native |
| `resources/cli/verify_composition_fillrule_diff.pagx` | native | native | native |
| `resources/cli/verify_composition_flex_diff.pagx` | native | native | native |
| `resources/cli/verify_composition_id_name_diff.pagx` | native | native | native |
| `resources/cli/verify_composition_layout_diff.pagx` | native | native | native |
| `resources/cli/verify_composition_matrix_diff.pagx` | native | native | native |
| `resources/cli/verify_composition_position_diff.pagx` | native | native | native |
| `resources/cli/verify_container_overflow.pagx` | native | native | native |
| `resources/cli/verify_content_at_origin.pagx` | native | native | native |
| `resources/cli/verify_content_offset.pagx` | native | native | native |
| `resources/cli/verify_content_offset_constrained.pagx` | native | native | native |
| `resources/cli/verify_content_offset_excluded_from_layout.pagx` | native | native | native |
| `resources/cli/verify_content_offset_explicit_size.pagx` | native | native | native |
| `resources/cli/verify_content_offset_flex.pagx` | native | native | native |
| `resources/cli/verify_content_offset_group.pagx` | native | native | native |
| `resources/cli/verify_content_offset_group_no_constraints.pagx` | native | native | native |
| `resources/cli/verify_content_offset_in_parent_layout.pagx` | native | native | native |
| `resources/cli/verify_content_offset_layer_unpositioned.pagx` | native | native | native |
| `resources/cli/verify_content_offset_mixed_constraints.pagx` | native | native | native |
| `resources/cli/verify_content_offset_neg.pagx` | native | native | native |
| `resources/cli/verify_content_offset_opposite_constraint.pagx` | native | native | native |
| `resources/cli/verify_downgradeable_layer.pagx` | native | native | native |
| `resources/cli/verify_downgradeable_layer_glass.pagx` | native | native | native |
| `resources/cli/verify_duplicate_gradient.pagx` | native | native | native |
| `resources/cli/verify_duplicate_pathdata.pagx` | native | native | native |
| `resources/cli/verify_element_constraint_conflict.pagx` | native | native | native |
| `resources/cli/verify_empty_nodes.pagx` | native | native | native |
| `resources/cli/verify_extractable_composition.pagx` | native | native | native |
| `resources/cli/verify_extractable_compositions.pagx` | native | native | native |
| `resources/cli/verify_flex_constraint_parent.pagx` | native | native | native |
| `resources/cli/verify_flex_constraint_zero_parent.pagx` | native | native | native |
| `resources/cli/verify_flex_horizontal.pagx` | native | native | native |
| `resources/cli/verify_flex_nested.pagx` | native | native | native |
| `resources/cli/verify_flex_no_parent_layout.pagx` | native | native | native |
| `resources/cli/verify_flex_no_parent_size.pagx` | native | native | native |
| `resources/cli/verify_flex_with_parent_size.pagx` | native | native | native |
| `resources/cli/verify_full_canvas_clip.pagx` | native | native | native |
| `resources/cli/verify_gap_no_layout.pagx` | native | native | native |
| `resources/cli/verify_gradient_colorstop_diff.pagx` | native | native | native |
| `resources/cli/verify_gradient_matrix_diff.pagx` | native | native | native |
| `resources/cli/verify_groups_different_cap.pagx` | native | native | native |
| `resources/cli/verify_groups_different_fill_color.pagx` | native | native | native |
| `resources/cli/verify_groups_mergeable_fillstroke.pagx` | native | native | native |
| `resources/cli/verify_include_in_layout_no_parent.pagx` | native | native | native |
| `resources/cli/verify_ineffective_center_explicit.pagx` | native | native | native |
| `resources/cli/verify_ineffective_center_group.pagx` | native | native | native |
| `resources/cli/verify_ineffective_center_group_constrained.pagx` | native | native | native |
| `resources/cli/verify_ineffective_center_layer.pagx` | native | native | native |
| `resources/cli/verify_invisible.pagx` | native | native | native |
| `resources/cli/verify_layout_overlap.pagx` | native | native | native |
| `resources/cli/verify_localizable_coords.pagx` | native | native | native |
| `resources/cli/verify_mergeable_adjacent_layers.pagx` | native | native | native |
| `resources/cli/verify_mergeable_groups.pagx` | native | native | native |
| `resources/cli/verify_negative_constraint.pagx` | native | native | native |
| `resources/cli/verify_not_xml.pagx` | reject (import/resource) | reject (import/resource) | reject (import/resource) |
| `resources/cli/verify_offcanvas.pagx` | native | native | native |
| `resources/cli/verify_padding_stretch.pagx` | native | native | native |
| `resources/cli/verify_padding_stretch_divider.pagx` | native | native | native |
| `resources/cli/verify_padding_stretch_nested.pagx` | native | native | native |
| `resources/cli/verify_painter_leak.pagx` | native | native | native |
| `resources/cli/verify_painter_leak_clean.pagx` | native | native | native |
| `resources/cli/verify_path_non_primitive.pagx` | native | native | native |
| `resources/cli/verify_path_to_primitive.pagx` | native | native | native |
| `resources/cli/verify_path_zero.pagx` | native | native | native |
| `resources/cli/verify_pathdata_different.pagx` | native | native | native |
| `resources/cli/verify_placeholder.pagx` | native | native | native |
| `resources/cli/verify_polystar_origin.pagx` | native | native | native |
| `resources/cli/verify_rect_mask_matching_frame.pagx` | native | native | native |
| `resources/cli/verify_redundant_centerx.pagx` | native | native | native |
| `resources/cli/verify_redundant_centery.pagx` | native | native | native |
| `resources/cli/verify_redundant_left_zero.pagx` | native | native | native |
| `resources/cli/verify_redundant_top_zero.pagx` | native | native | native |
| `resources/cli/verify_simple.pagx` | native | native | native |
| `resources/cli/verify_text_fallback.pagx` | native | native | native |
| `resources/cli/verify_unknown_attribute.pagx` | native | native | native |
| `resources/cli/verify_unref_resources.pagx` | native | native | native |
| `resources/cli/verify_unwrappable_group.pagx` | native | native | native |
| `resources/cli/verify_zero_size_deep_leaf.pagx` | native | native | native |
| `resources/cli/verify_zero_size_empty.pagx` | native | native | native |
| `resources/cli/verify_zero_size_explicit.pagx` | native | native | native |
| `resources/cli/verify_zero_size_no_leaf.pagx` | native | native | native |
| `resources/layout/constraint_absolute.pagx` | native | native | native |
| `resources/layout/constraint_ellipse.pagx` | native | native | native |
| `resources/layout/constraint_group.pagx` | native | native | native |
| `resources/layout/constraint_layer.pagx` | native | native | native |
| `resources/layout/constraint_padding.pagx` | native | native | native |
| `resources/layout/constraint_path.pagx` | native | native | native |
| `resources/layout/constraint_polystar.pagx` | native | native | native |
| `resources/layout/constraint_rectangle.pagx` | native | native | native |
| `resources/layout/constraint_text.pagx` | native | native | native |
| `resources/layout/constraint_textbox.pagx` | native | native | native |
| `resources/layout/container_compound.pagx` | native | native | native |
| `resources/layout/container_edge_cases.pagx` | native | native | native |
| `resources/layout/container_hug.pagx` | native | native | native |
| `resources/layout/container_vertical.pagx` | native | native | native |
| `resources/layout/flex.pagx` | native | native | native |
| `resources/layout/padding_unified.pagx` | native | native | native |
| `resources/layout/textbox_constraint_wrap.pagx` | native | native | native |
| `resources/layout/textbox_explicit_height.pagx` | native | native | native |
| `resources/layout/textbox_flex_sibling.pagx` | native | native | native |
| `resources/layout/textbox_nested_wrap.pagx` | native | native | native |
| `resources/pagx_to_html/blend_modes_showcase.pagx` | native | native | native |
| `resources/pagx_to_html/clip_and_mask.pagx` | native | native | native |
| `resources/pagx_to_html/color_conic_gradient.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/color_diamond_gradient.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/color_formats.pagx` | native | native | native |
| `resources/pagx_to_html/color_image_pattern.pagx` | bake | bake | bake |
| `resources/pagx_to_html/color_linear_gradient.pagx` | native | native | native |
| `resources/pagx_to_html/color_p3.pagx` | native | native | native |
| `resources/pagx_to_html/color_radial_gradient.pagx` | native | native | native |
| `resources/pagx_to_html/composition_advanced.pagx` | native | native | native |
| `resources/pagx_to_html/coverage_gaps.pagx` | reject (unsupported-feature) | bake | bake |
| `resources/pagx_to_html/data_attributes.pagx` | native | native | native |
| `resources/pagx_to_html/drop_shadow_show_behind.pagx` | native | native | native |
| `resources/pagx_to_html/edge_defaults.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/fillrule_comparison.pagx` | native | native | native |
| `resources/pagx_to_html/filter_all.pagx` | native | native | native |
| `resources/pagx_to_html/filter_chain.pagx` | native | native | native |
| `resources/pagx_to_html/font_subset_woff2.pagx` | native | native | native |
| `resources/pagx_to_html/geometry_ellipse.pagx` | native | native | native |
| `resources/pagx_to_html/geometry_path.pagx` | native | native | native |
| `resources/pagx_to_html/geometry_polystar.pagx` | native | native | native |
| `resources/pagx_to_html/geometry_rectangle.pagx` | native | native | native |
| `resources/pagx_to_html/gradient_matrix.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/group.pagx` | native | native | native |
| `resources/pagx_to_html/image_external_and_base64.pagx` | bake | bake | bake |
| `resources/pagx_to_html/layer_3d_multi_axis.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/layer_alpha.pagx` | native | native | native |
| `resources/pagx_to_html/layer_antialias.pagx` | native | native | native |
| `resources/pagx_to_html/layer_blend_modes.pagx` | native | native | native |
| `resources/pagx_to_html/layer_group_opacity.pagx` | native | native | native |
| `resources/pagx_to_html/layer_nesting.pagx` | native | native | native |
| `resources/pagx_to_html/layer_pass_through.pagx` | native | native | native |
| `resources/pagx_to_html/layer_scroll_rect.pagx` | native | native | native |
| `resources/pagx_to_html/layer_transform_matrix.pagx` | native | native | native |
| `resources/pagx_to_html/layer_transform_matrix3d.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/layer_transform_priority.pagx` | native | native | native |
| `resources/pagx_to_html/layer_transform_xy.pagx` | native | native | native |
| `resources/pagx_to_html/layer_visibility.pagx` | native | native | native |
| `resources/pagx_to_html/layout_arrangement.pagx` | native | native | native |
| `resources/pagx_to_html/modifier_merge_path.pagx` | native | native | native |
| `resources/pagx_to_html/modifier_round_corner.pagx` | native | native | native |
| `resources/pagx_to_html/modifier_trim_continuous.pagx` | native | native | native |
| `resources/pagx_to_html/modifier_trim_path.pagx` | native | native | native |
| `resources/pagx_to_html/painter_accumulated_geometry.pagx` | native | native | native |
| `resources/pagx_to_html/painter_fill.pagx` | native | native | native |
| `resources/pagx_to_html/painter_multiple.pagx` | native | native | native |
| `resources/pagx_to_html/painter_stroke.pagx` | native | native | native |
| `resources/pagx_to_html/polystar_advanced.pagx` | native | native | native |
| `resources/pagx_to_html/repeater.pagx` | native | native | native |
| `resources/pagx_to_html/resource_composition.pagx` | native | native | native |
| `resources/pagx_to_html/resource_forward_ref.pagx` | native | native | native |
| `resources/pagx_to_html/root_document.pagx` | native | native | native |
| `resources/pagx_to_html/selector_advanced.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/selector_unit_percentage.pagx` | native | native | native |
| `resources/pagx_to_html/shape_glyph_run.pagx` | bake | bake | bake |
| `resources/pagx_to_html/showcase_dashboard_card.pagx` | native | native | native |
| `resources/pagx_to_html/showcase_infographic.pagx` | reject (unsupported-feature) | bake | bake |
| `resources/pagx_to_html/showcase_mandala.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/showcase_text_art.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/stroke_advanced.pagx` | native | native | native |
| `resources/pagx_to_html/stroke_align.pagx` | native | native | native |
| `resources/pagx_to_html/stroke_cap_join.pagx` | native | native | native |
| `resources/pagx_to_html/stroke_dash_adaptive.pagx` | native | native | native |
| `resources/pagx_to_html/stroke_dash_offset_cycle.pagx` | native | native | native |
| `resources/pagx_to_html/style_filter_advanced.pagx` | native | native | native |
| `resources/pagx_to_html/style_shadows.pagx` | native | native | native |
| `resources/pagx_to_html/text_baseline.pagx` | native | native | native |
| `resources/pagx_to_html/text_basic.pagx` | native | native | native |
| `resources/pagx_to_html/text_box.pagx` | native | native | native |
| `resources/pagx_to_html/text_box_align.pagx` | native | native | native |
| `resources/pagx_to_html/text_fauxbold_stroke.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/text_features.pagx` | native | native | native |
| `resources/pagx_to_html/text_modifier.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/text_modifier_advanced.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/text_path.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/text_path_advanced.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/text_path_circle.pagx` | reject (unsupported-feature) | bake | native |
| `resources/pagx_to_html/text_rich.pagx` | native | native | native |
| `resources/pagx_to_html/text_writing_mode.pagx` | native | native | native |
| `resources/pagx_to_html/unit/composition_filter_size.pagx` | native | native | native |
| `resources/pagx_to_html/unit/custom_cff_charset_strings.pagx` | native | native | native |
| `resources/pagx_to_html/unit/drop_shadow_styles.pagx` | native | native | native |
| `resources/pagx_to_html/unit/fixed_flex_item_scroll.pagx` | native | native | native |
| `resources/pagx_to_html/unit/flex_child_layer_width.pagx` | native | native | native |
| `resources/pagx_to_html/unit/glyph_run_embedded_font.pagx` | native | native | native |
| `resources/pagx_to_html/unit/image_pattern_scale_none_background_size.pagx` | bake | bake | bake |
| `resources/pagx_to_html/unit/large_charstring_operands_font.pagx` | native | native | native |
| `resources/pagx_to_html/unit/low_units_per_em_font.pagx` | native | native | native |
| `resources/pagx_to_html/unit/merge_path_fill_rule.pagx` | native | native | native |
| `resources/pagx_to_html/unit/mixed_glyph_run_fonts.pagx` | native | native | native |
| `resources/pagx_to_html/unit/parent_child_drop_shadow.pagx` | native | native | native |
| `resources/pagx_to_html/unit/rectangle_stroke_roundness.pagx` | native | native | native |
| `resources/pagx_to_html/unit/rich_text_newline_group.pagx` | native | native | native |
| `resources/pagx_to_html/unit/scroll_rect_flex_flow.pagx` | native | native | native |
| `resources/pagx_to_html/unit/single_auto_size_glyph_run.pagx` | native | native | native |
| `resources/pagx_to_html/unit/single_group_text_box.pagx` | native | native | native |
| `resources/pagx_to_html/unit/text_box_glyph_run_emoji_fallback.pagx` | native | native | native |
| `resources/pagx_to_html/unit/uniform_gradient_text.pagx` | native | native | native |
| `resources/pagx_to_html/unit/vertical_scroll_rect_height.pagx` | native | native | native |
| `resources/pagx_to_html/unit/vertical_text_box_height.pagx` | native | native | native |
| `resources/ppt/cases/smoke/native-text.pagx` | native | native | native |
| `resources/ppt/cases/smoke/solid-fill.pagx` | native | native | native |
| `resources/text/baseline.pagx` | native | native | native |
| `resources/text/bidi_basic.pagx` | native | native | native |
| `resources/text/box_layout.pagx` | native | native | native |
| `resources/text/emoji.pagx` | native | native | native |
| `resources/text/group_matrix.pagx` | native | native | native |
| `resources/text/line_break.pagx` | native | native | native |
| `resources/text/punctuation_squash.pagx` | native | native | native |
| `resources/text/special_chars.pagx` | native | native | native |
| `resources/text/text_reshape_demo.pagx` | native | native | native |
| `resources/text/vertical.pagx` | native | native | native |
