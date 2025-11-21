; Capture HTML normal elements that lack an explicit end tag.
; We check that the last child of the element is not an end_tag node;
; elements closed implicitly (including at EOF) will match.
((normal_element
  (tag_name) @open) @unclosed
 (#not-match? @unclosed "</[^<]*>$"))
