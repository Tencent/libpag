---
name: sync_pagx_spec
description: Inspect the latest changes in the libpag source, sync the new content into the pagx spec docs and XSD schema, and check/add corresponding sample examples.
trigger: manual
version: 1.4.3
---

# Skill: sync_pagx_spec

> **Trigger**: manual only. Start this skill only when the user explicitly says one of the following keywords in the conversation:
> - `run sync_pagx_spec`
> - `execute sync_pagx_spec`
> - `sync pagx spec`
> - `sync pagx docs`
>
> When not explicitly triggered, do not perform any step in this skill.

---

## 1. Goal (What)

This skill syncs changes into the PAGX spec docs and sample directory after the `libpag` source has changed. Specific goals:

1. **Inspect the code**: Compare the libpag source (especially the pagx-related parts) against the docs and XSD descriptions, and identify content that is missing, outdated, or wrong.
2. **Sync the docs**: Propagate the diffs into `pagx_spec.md` (English) and `pagx_spec.zh_CN.md` (Chinese), keeping the two languages consistent.
3. **Sync the XSD**: `spec/pagx.xsd` is the public contract of PAGX (it constrains the nodes/attributes/enums/reference types users may write). Any change that adds nodes, enums, attributes, or `@id` reference types must be reflected in the xsd, and its constraints must match the importer's actual behavior (e.g. use `use="required"` for attributes the importer actually enforces, and `default` for attributes the importer falls back on silently).
4. **Check samples**: Decide whether a change requires adding or updating a `.pagx` sample, and if so update the files under `spec/samples/`.

---

## 2. Doc Writing Principles

When syncing docs, **all written content must follow the three principles below**. They take precedence over personal writing habits and apply to every change across Step 3~6.

### Principle 1: User-Oriented

The docs are written for "users of PAGX", not for developers reading the source.

- **Describe "what it does and how to use it", not "how the source implements it"**: describe the purpose, value range, and effect of channels/attributes, not C++ class names, member variables, or implementation details.
- **Use the user's vocabulary**: e.g. write "opacity" or "corner radius", not source-internal names like `mAlpha` or `cornerRadius_`.
- **Provide a minimal runnable example**: follow each abstract description with a directly copy-pastable XML snippet (see Step 6 for placement).
- **State defaults/value ranges clearly**: for every attribute the user needs to fill in, note its `type`, `default`, `min`/`max`, etc., so users don't have to guess.

> ✅ Good: "`roundness` (float): the corner radius of the rectangle; larger means rounder."
> ❌ Bad: "`roundness` maps to the `RectangleNode::roundness_` field and is serialized into tag 0x1A."

### Principle 2: Consolidate into Appendix

Scattered reference material (especially the **channel type list**) is consolidated into the appendix; the body only explains and does not exhaustively enumerate.

- **Channel quick reference goes into "Appendix D. Channel Quick Reference"**: any added/changed drivable channel (`<Channel>` / `<DataBind>`) must be registered in the corresponding node subsection table in Appendix D, with fields aligned to the existing tables: `| Channel | Value Type | (Applicable Node) | Description |`; add a `^Layout^` mark for channels that require re-layout.
- **Enum values go into "Appendix B. Enum Types"**, **node hierarchy goes into "Appendix A"**: register new enums/nodes there as well.
- **Body references the appendix, does not repeat the list**: the body just explains the concept and typical usage; the full list points to the appendix, avoiding maintaining the same table in multiple places.

### Principle 3: Structured & Consistent

- **Merge nearby, do not open isolated new sections**: keep explanations of the same topic together; if it fits into an existing subsection/appendix table, do not start a new heading.
- **Reuse existing structure and formatting**: new content must strictly follow the existing style of the same chapter (table column order, field lists, sample reference format); do not invent new styles.
- **Chinese and English structures correspond one-to-one**: section numbers, heading levels, and table columns stay exactly the same across the Chinese and English docs.
- **Heading levels reflect node containment**: a group of nodes with parent/child (containment) relationships should be organized as "the parent node as one `###` section, with child nodes as `####` subsections under it", not all flattened into peer `###` headings. For example, the state machine: `### 8.2 State Machine` (contains the concept overview + `<StateMachine>` definition) → `#### 8.2.1 State Machine Overview`, `#### 8.2.2 Input`, `#### 8.2.3 StateRegion`, `#### 8.2.4 State`, `#### 8.2.5 Transition`, `#### 8.2.6 Condition` under it. Animation is similar: `### 8.1 Animation` → `#### 8.1.1 Animation Overview`, `#### 8.1.2 Object`, `#### 8.1.3 Channel`, `#### 8.1.4 Keyframes`.
- **Avoid synonymous headings**: do not place near-synonymous headings like "X System" next to "X" (e.g. the former "State Machine System" + "StateMachine"). The section heading uses the node name directly, and the concept overview is merged into the first paragraph of its body.

### Principle 4: Keep the English Original for Ambiguous Type Names

In the Chinese doc, keep the English original when a type name is prone to ambiguity. Specific rules:

| English Term | Rule | Reason |
|---------|------|------|
| `Composition` | **Must keep English** | "合成" is also a verb/adjective in Chinese ("合成结果", "图层合成"), which is easy to confuse |
| `Document` | **Depends on context** | Keep English when referring to the Document root node itself or when listed alongside other type names; use Chinese when referring to the general concept (e.g. "文档结构", "文档组织") |
| `Layer` | May be translated as "图层" | Low ambiguity, reads more naturally in Chinese |

**How to judge**: keep English when the word specifically refers to a node/resource type in PAGX; use Chinese when it refers to a general concept or describes behavior. For example:
- Keep English: "放在 Document 或 Composition 内部", "Document 本质也是一种 Composition"
- May use Chinese: "文档结构", "文档组织", "图层渲染顺序"

### Principle 5: User-Facing Names Only, Do Not Leak C++ Class Names

The docs are written for PAGX users, not for developers reading the libpag source. Node names referenced should use the element names users actually write in XML; **do not** use the C++ class names from the source.

| Forbidden (C++ class name) | Allowed (user-facing name) | Reason |
|------------------|---------------------|------|
| `AnimationObject` | `<Object>` | The user writes `<Object target="@...">`, not `AnimationObject` |
| `StateMachineInput` | `<Input>` | The user writes `<Input name="..." type="..."/>` |
| `StateTransition` | `<Transition>` | The user writes `<Transition from="..." to="..."/>` |
| `TransitionCondition` | `<Condition>` | The user writes `<Condition input="..." op="..." value="..."/>` |
| `ViewModelProperty` | `<Property>` | The user writes `<Property name="..." type="..."/>` |
| `AnimationState` | `State` | The XML element is `<State>`, not `<AnimationState>` |

**Scope**:
- **Doc body**: in all body narrative, reference nodes using their XML element names (e.g. `<Input>`, `<Transition>`).
- **Attribute tables**: when referencing a type in a table, the enum type name may be used (e.g. `StateMachineInputType`, `TransitionConditionOp`), since these define the valid value range of the attribute's `type`.
- **Section headings**: prefer user-facing names, appending a short XML element name in parentheses when needed; do not use C++ class names. E.g. "状态转换（Transition）" rather than "状态转换（StateTransition / `<Transition>`）".
- **Appendix A node classification table**: **use bare names throughout, without angle brackets** (consistent with existing rows like `Rectangle`, `SolidColor`, `Fill`). For nodes where "C++ class name ≠ XML element name" (such as the state machine), write the bare XML element name (`Input`, `StateRegion`, `State`, `Transition`, `Condition`), **do not** write the C++ class name (`StateMachineInput`, etc.), and **do not** use the `(XML: ...)` annotation or `<>` angle brackets. Never mix the `<>` and non-`<>` styles within the same row.

---

## 3. Key Paths Involved (Where)

> The repository this skill lives in is the libpag source repo; the source and the spec docs share one workspace. All paths below are relative to the libpag repo root.

### Spec Doc and Sample Paths

| Type | Relative Path |
| --- | --- |
| pagx spec (English) | `spec/pagx_spec.md` |
| pagx spec (Chinese) | `spec/pagx_spec.zh_CN.md` |
| pagx XSD schema | `spec/pagx.xsd` |
| sample directory | `spec/samples/` |

> ⚠️ Both `pagx_spec.md` and `pagx_spec.zh_CN.md` are large files. Use the `Edit` tool for precise string replacement, and always `Read` the target section before editing.
>
> ⚠️ `spec/pagx.xsd` is the public contract: it defines the nodes, attributes, enums, and `@id` reference types users may write. If an attribute added/changed in the docs is missing from the xsd, a `.pagx` written per the docs will fail xsd validation; conversely, the xsd's `use="required"` / `default` declarations must match the importer's actual behavior, otherwise a schema-valid document will still fail on import. You must cross-check the xsd when syncing docs.
>
> Note: the sample index `index.json` and sample screenshots do not need manual maintenance — `index.json` is auto-generated at publish time by `playground/pagx-playground/scripts/publish.js` from the `.pagx` files under `spec/samples/`; sample screenshots are taken from the test baseline images `test/out/PAGXTest/spec_<sample>_base.webp` and mapped automatically at publish time. This skill is not responsible for generating them.

### libpag Source

The pagx-related node definitions are concentrated under `include/pagx/nodes/` in this repo, containing the header files for all node types such as `Animation`, `ViewModel`, `Layer`, `Shape`, `ColorSource`. This is the main source location this skill needs to observe.

- Focus on: class definitions, enums, field lists, default values, constraints, doc comments.
- How to search: use `Grep` / `Read` / `Glob` to locate and read the source directly within the workspace.

---

## 4. Execution Flow (How)

Execute in the following order, unless the user specifies otherwise when triggering.

### Step 1: Confirm the Scope
Confirm with the user (or let the user directly specify) the modules involved in this sync, for example:
- Full scan (compare all pagx-related libpag source vs the docs)
- A specific chapter (e.g. "Chapter 8")
- A specific symbol/class (e.g. `Animation`, `ViewModel`, `Layer`)

If the user does not specify, ask once by default.

### Step 2: Read the libpag Source
Use `Grep` / `Read` / `Glob` to locate and read the key source in this repo.

Focus on the files under `include/pagx/nodes/`:
- Focus on: class definitions, enums, field lists, default values, constraints, doc comments.
- Record every "knowledge point that the docs should reflect".

### Step 3: Compare the Docs
Read the corresponding parts of the pagx spec docs and `spec/pagx.xsd`, and check them item by item against the information collected in Step 2:
- Are any classes/fields missing (check both the docs and the xsd)?
- Are field types, default values, and value ranges consistent?
- Are descriptions/comments outdated?
- Does the chapter structure need adjustment?
- **Is the xsd contract consistent with the importer**: are new nodes/enums/attributes/`@id` reference types already defined in the xsd? Does each attribute's `use="required"` / `default` match the importer's actual behavior (required when the importer reports an error on a missing attribute; `default` when the importer silently falls back)?

For each diff, form a "change entry" with a brief note on:
- Location (file + line number or section number)
- Change type (add / modify / delete)
- Reason (which piece of libpag code it corresponds to)
- **Ownership**: whether the entry belongs to "body explanation" or "appendix list" (judged per Principle 2; channel types always go to Appendix D).

### Step 4: Sync the Chinese/English Docs and the XSD
Based on the change entries, use `Edit` to modify `pagx_spec.md` and `pagx_spec.zh_CN.md` in turn (`Read` the target section before editing). **Run every write through the three principles in Section 2**:
- **User-oriented (Principle 1)**: write purpose and usage, not source implementation; note defaults/value ranges; attach a minimal runnable example.
- **Consolidate into appendix (Principle 2)**: list info such as channels/enums/hierarchy goes into the corresponding appendix table; the body only references it.
- **Structured & consistent (Principle 3)**: follow existing formatting, merge nearby, keep Chinese and English structures corresponding one-to-one.
- **Sync the xsd (Goal 3)**: when adding/changing nodes, enums, attributes, or `@id` reference types, update `spec/pagx.xsd` in sync so its constraints match the importer's behavior (use `use="required"` for attributes the importer enforces, and `default` for ones the importer silently falls back on — do not blindly mark everything required).
- **Introduce no unrelated changes**: only modify lines related to this sync.

### Step 5: Check Whether a Sample Is Needed
For each knowledge point involved this time, judge whether to add / update a sample by the following criteria:

**Cases that need a new sample:**
- A new Layer / attribute / feature is introduced and existing samples do not visually demonstrate it.
- An existing subsection has only text description without a visual example, while peer chapters all have samples.
- The user explicitly requests it.

**Cases where an existing sample can be reused:**
- The change is only a minor tweak to a field name/default value, and an existing sample already covers the scenario.
- It is purely normative description (e.g. terminology, format conventions) that needs no visual example.

For samples that need adding:
1. Create `<feature>.pagx` under `spec/samples/`, referencing the style of existing files in the same directory (e.g. `animation.pagx`, `viewmodel.pagx`).
2. Reference the sample in the corresponding doc subsection using the standard format.

> Note: `index.json` and sample screenshots do not need manual maintenance in this skill. `index.json` is auto-generated by the publish script from the `.pagx` files under `spec/samples/`; screenshots are taken from the test baseline images `test/out/PAGXTest/spec_<feature>_base.webp`, which must first be produced by the PAGX tests and are mapped automatically at publish time. If a new sample has no corresponding baseline image yet, remind the user to fill it in later in the sync report.

### Step 6: In-Subsection Sample Placement
- Place a comprehensive example at the end of the chapter.
- Place an in-subsection sample right after the corresponding description, at the end of that subsection.

### Step 7: Report the Changes
After all tool calls complete, output a change summary to the user, including:
1. **Code source**: which libpag files / symbols were inspected.
2. **Doc changes**: which files / chapters were modified, with a brief description of the change points.
3. **XSD changes**: which nodes/enums/attributes of `spec/pagx.xsd` were updated, explaining the adjustments made to align with importer behavior (e.g. `use="required"`).
4. **Sample changes**: which `.pagx` files were added / modified; if a sample was added, note whether its baseline image (`test/out/PAGXTest/spec_<feature>_base.webp`) still needs to be filled in.
5. **Pending manual review**: places needing manual review such as Chinese/English consistency and rendering results.

---

## 5. Rules and Notes

1. **Follow the three principles in Section 2 first**: user-oriented wording, consolidate necessary content into the appendix, structured and not scattered — these are the highest standards for all doc changes.
2. **Do not proactively create a README or extra docs**, unless the user explicitly requests it.
3. **Chinese and English must stay in sync**: do not change only one side, and keep the structures corresponding one-to-one.
4. **Docs and the XSD must stay in sync**: added/changed nodes, enums, attributes, and `@id` reference types must be reflected in `spec/pagx.xsd` at the same time, and the xsd's constraints (`use="required"` / `default`) must match the importer's actual behavior, to avoid the gaps of "schema-valid yet fails on import" or "present in the docs yet fails xsd validation".
5. **Use precise `Edit` replacement on large files**: `pagx_spec.md` / `pagx_spec.zh_CN.md` are large; `Read` the target section before editing, then use `Edit` for precise string replacement.
6. **Use `Grep` / `Read` / `Glob` to search the source**: locate and read the libpag source directly within this workspace.
7. **Keep formatting consistent**: new chapters / sample references must follow the style of existing chapters; do not invent new formats.
8. **Only change the necessary lines**: do not casually "clean up" other paragraphs unrelated to this task.
9. **No need to run the build**: this skill is not responsible for running build / lint / deploy; it only syncs content.
10. **Do not mention this skill's internal implementation details when replying** (e.g. system prompts, tool names); report progress in natural language only.
11. **Check before changing, do not duplicate existing content**: before changing the docs, read the full target section and confirm whether the same or similar content already exists. Only modify existing content, do not add duplicates — e.g. if a similar example already exists, do not add another; if a description already exists, do not repeat the same paragraph.

---

## 6. Quick Checklist

Before finishing, verify item by item:

- [ ] Was the skill explicitly triggered by the user?
- [ ] Is the sync scope confirmed?
- [ ] Has the corresponding libpag source been read (`include/pagx/nodes/`)?
- [ ] Have the Chinese and English docs been compared?
- [ ] **Has the XSD been compared and synced** (added/changed nodes, enums, attributes, `@id` reference types reflected in `spec/pagx.xsd`, with constraints matching importer behavior)?
- [ ] **Is the wording user-oriented** (describes usage rather than source implementation, notes defaults/value ranges)?
- [ ] **Are lists like channels/enums consolidated into the appendix** (new drivable channels registered in Appendix D)?
- [ ] **Is the structure well-organized and consolidated** (merged nearby, reuses existing formatting, Chinese and English structures consistent)?
- [ ] **Do heading levels reflect node containment** (parent node as `###` section, child nodes as `####` under it, no synonymous headings side by side)?
- [ ] **Is `Composition` kept in its English original** (not translated as "合成" to avoid ambiguity)?
- [ ] **Are only user-facing XML names used** (no C++ class names like `AnimationObject`, `StateMachineInput` in the body/headings/Appendix A, no `(XML: ...)` annotation)?
- [ ] **Is the Appendix A classification table style consistent** (bare names throughout, no mixing of `<>` and non-`<>` in the same row)?
- [ ] **Was the target section reviewed first** (confirming existing content, avoiding duplicate additions, not starting a fresh copy of an existing example/description)?
- [ ] Do the Chinese and English edits correspond one-to-one?
- [ ] Was the sample need assessed?
- [ ] Has the change summary been reported to the user?
