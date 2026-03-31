# Wallet UI Quality Pass — Design System Alignment + Publish Wizard

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring all wallet pages to production-quality shadcn/ui design system compliance, redesign the Publish tab as a multi-step wizard matching the reference implementation, and eliminate all remaining inconsistencies.

**Architecture:** Pure CSS design system using shadcn/ui oklch CSS variables (`:root`/`.dark` blocks). No Tailwind runtime (Ladybird LibJS doesn't support it). All icons are inline SVGs (Lucide paths). HTML pages served as static resources from `Base/res/ladybird/about-pages/`. JS handles interaction via `ladybird.sendMessage()` WebUI bridge.

**Tech Stack:** HTML5, CSS3 (custom properties, grid, flexbox), vanilla JS, Lucide SVG icons, shadcn/ui design tokens

**Key constraints:**
- No React, no Tailwind runtime, no JS frameworks
- Ladybird's LibJS has limited API support — test everything
- All colors MUST use `var(--background)`, `var(--card)`, etc. — zero hardcoded values
- All border-radius MUST use `var(--radius)` calculations
- All font sizes in rem (no px)
- No emoji icons — inline SVG only

---

## Wave 1: Design System Cleanup (3 tasks, parallel)

### Task 1: Eliminate all inline styles from wallet.html

**Files:**
- Modify: `Base/res/ladybird/about-pages/wallet.html`

Currently 28 `style=` attributes remain. Convert each to a CSS class.

- [ ] **Step 1:** Read the full wallet.html. Find every `style=` attribute.
- [ ] **Step 2:** For each inline style, either:
  - Add a CSS class in the `<style>` block if reusable (e.g. `.max-w-sm { max-width: 24rem; }`)
  - Use existing utility classes if they cover the property
- [ ] **Step 3:** Remove all `style=` attributes from HTML elements.
- [ ] **Step 4:** Verify zero `style=` attributes remain: `grep -c "style=" wallet.html` should return 0
- [ ] **Step 5:** Deploy to build dir, relaunch Ladybird, visually verify no layout breaks
- [ ] **Step 6:** Commit: `git commit -m "Remove all inline styles from wallet.html"`

### Task 2: Fix remaining px font sizes and add missing CSS classes

**Files:**
- Modify: `Base/res/ladybird/about-pages/wallet.html`

4 px-based font sizes remain. Also audit for any hardcoded hex colors.

- [ ] **Step 1:** Find all `font-size:.*px` and replace with rem equivalents from the type scale
- [ ] **Step 2:** Find any hardcoded hex colors (`#xxx`) and replace with `var(--token)`
- [ ] **Step 3:** Ensure every interactive element has `:focus-visible { outline: 2px solid var(--ring); outline-offset: 2px; }`
- [ ] **Step 4:** Commit: `git commit -m "Fix px sizes and add focus-visible ring"`

### Task 3: Align newtab.html with same design system

**Files:**
- Modify: `Base/res/ladybird/about-pages/newtab.html`

- [ ] **Step 1:** Audit newtab for inline styles, px fonts, hardcoded colors
- [ ] **Step 2:** Fix all issues found
- [ ] **Step 3:** Ensure newtab Item cards match the app-card pattern exactly
- [ ] **Step 4:** Commit: `git commit -m "Align newtab design system with wallet"`

---

## Wave 2: Publish Wizard Redesign (1 large task)

### Task 4: Build multi-step Publish wizard

**Files:**
- Modify: `Base/res/ladybird/about-pages/wallet.html` (HTML + CSS)
- Modify: `Base/res/ladybird/about-pages/wallet/wallet.js` (wizard logic)

Reference: `~/code/1sat-sdk/packages/wallet-desktop/src/mainview/views/publish/index.tsx` (1827 lines)

The wizard has 4 steps with a step indicator bar:

**Step 1 — Content Type Picker:**
- 4 cards: Image, Video, Document, HTML App
- Each card has icon, title, description
- Single file types trigger file picker immediately
- HTML App advances to Step 2

**Step 2 — Select Project (HTML App only):**
- Directory path input
- Build tool auto-detection (Vite/CRA/Static)
- Recent projects list
- For Bottlebird: this step shows a message that project scanning requires native integration (MCP/filesystem access)

**Step 3 — Configure:**
- App name input
- Description textarea
- OpNS name selector (dropdown of owned names)
- Identity display (auto-filled from wallet)
- Permissions checklist (getPublicKey, createAction, etc.)
- Cost estimate breakdown (inscription + MAP + AIP + miner fees)

**Step 4 — Publish:**
- Review summary of all config
- Balance check (sufficient funds?)
- "Inscribe" button
- Broadcasting state with spinner
- Success state with outpoint link and OpNS URL

**Implementation approach:**

- [ ] **Step 1:** Add step indicator CSS
```css
.wizard-steps { display: flex; align-items: center; gap: 0; padding: 0.75rem 1rem; border-bottom: 1px solid var(--border); }
.wizard-step { display: flex; align-items: center; gap: 0.5rem; font-size: 0.75rem; font-weight: 500; color: var(--muted-foreground); }
.wizard-step.active { color: var(--foreground); }
.wizard-step.complete { color: var(--success); }
.wizard-step-number { width: 1.5rem; height: 1.5rem; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 0.6875rem; font-weight: 600; border: 1px solid var(--border); }
.wizard-step.active .wizard-step-number { background: var(--primary); color: var(--primary-foreground); border-color: var(--primary); }
.wizard-step.complete .wizard-step-number { background: var(--success); color: white; border-color: var(--success); }
.wizard-chevron { color: var(--muted-foreground); margin: 0 0.25rem; }
```

- [ ] **Step 2:** Add content type picker HTML
```html
<div id="publish-step-picker" class="grid-2">
  <!-- 4 content type cards with SVG icons -->
</div>
```

- [ ] **Step 3:** Add configure step HTML (name, description, OpNS, identity, permissions, cost)
- [ ] **Step 4:** Add publish/review step HTML (summary, balance check, inscribe button, success state)
- [ ] **Step 5:** Add JS wizard state machine
```javascript
let publishStep = "picker"; // picker | configure | publish | success
function setPublishStep(step) { ... }
```
- [ ] **Step 6:** Wire file picker for single-file types
- [ ] **Step 7:** Wire configure form validation
- [ ] **Step 8:** Wire publish button (disabled until configured, shows "Requires wallet backend" for now)
- [ ] **Step 9:** Deploy and test all wizard steps
- [ ] **Step 10:** Commit: `git commit -m "Add multi-step Publish wizard matching reference implementation"`

---

## Wave 3: Review + Polish (2 tasks, parallel)

### Task 5: Visual review with Gemini

- [ ] **Step 1:** Take screenshots of every tab (Dashboard, Ordinals, Tokens, Market, Apps, Identity, Send, Receive, Chat, Publish)
- [ ] **Step 2:** Send to Gemini for design review
- [ ] **Step 3:** Fix all issues identified
- [ ] **Step 4:** Commit fixes

### Task 6: Cross-tab consistency audit

- [ ] **Step 1:** Verify every tab uses identical card/header/body patterns
- [ ] **Step 2:** Verify every empty state uses identical structure (SVG icon + h3 + p)
- [ ] **Step 3:** Verify every form uses identical input/label/button patterns
- [ ] **Step 4:** Verify nav tab active state is consistent
- [ ] **Step 5:** Fix any inconsistencies found
- [ ] **Step 6:** Final commit and push

---

## Verification Checklist

After all waves complete:
- [ ] Zero `style=` inline attributes in wallet.html
- [ ] Zero px-based font sizes
- [ ] Zero hardcoded hex colors
- [ ] Zero emoji/unicode entity icons
- [ ] All interactive elements have focus-visible ring
- [ ] All border-radius uses var(--radius)
- [ ] All colors use semantic tokens
- [ ] Publish wizard has 4 working steps
- [ ] All tabs render correctly in Ladybird
- [ ] Git pushed to origin
