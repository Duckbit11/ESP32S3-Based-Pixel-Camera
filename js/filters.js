const paletteContainer = document.getElementById("palette_container");
const paletteNameInput = document.getElementById("palette_name");
const customHexInput = document.getElementById("custom_hex_input");
const applyCustomButton = document.getElementById("apply_custom_palette");

let palettes = [];
let editingPaletteIndex = null;

function getPaletteDataUrl() {
  return new URL("../palettes.json", window.location.href).href;
}

async function loadDefaultPalettes() {
  try {
    const response = await fetch(getPaletteDataUrl());
    if (!response.ok) {
      throw new Error("Unable to load palette data");
    }

    const data = await response.json();
    palettes = Array.isArray(data.palettes) ? data.palettes : [];
    renderPalettes();
  } catch (error) {
    console.error("Failed to load palettes from JSON:", error);
    palettes = [];
    renderPalettes();
  }
}

function formatPaletteName(name) {
  return name && name.trim() ? name.trim() : "Custom Palette";
}

function normalizeHex(color) {
  const clean = color.trim();
  if (!clean) return null;

  const hex = clean.startsWith("#") ? clean : `#${clean}`;
  const normalized = hex.replace(/[^#0-9a-fA-F]/g, "");
  const pureHex = normalized.replace(/^#+/, "#");

  if (!pureHex || pureHex.length < 4 || pureHex.length > 7) {
    return null;
  }

  if (pureHex.length === 4) {
    return `#${pureHex.slice(1).split("").map((char) => char + char).join("")}`.toLowerCase();
  }

  return pureHex.toLowerCase();
}

function parseCustomHexes(rawText) {
  const tokens = rawText
    .split(/[\s,./]+/)
    .map((entry) => entry.trim())
    .filter(Boolean);

  const values = tokens
    .map((token) => normalizeHex(token))
    .filter(Boolean);

  return values;
}

function buildPaletteCard(palette) {
  const swatches = Array.from({ length: 8 }, (_, index) => {
    const color = palette.colors[index] || palette.colors[0] || "#ffffff";
    return `<span class="palette_swatch" style="background:${color};" title="${color}"></span>`;
  }).join("");

  return `
    <article class="palette_card">
      <div class="palette_name_row">
        <h3>${palette.name}</h3>
      </div>
      <div class="palette_swatches">${swatches}</div>
    </article>
  `;
}

function resetCustomForm() {
  editingPaletteIndex = null;
  applyCustomButton.textContent = "Apply custom palette";
}

function preparePaletteForEditing(index) {
  const palette = palettes[index];
  if (!palette) return;

  editingPaletteIndex = index;
  paletteNameInput.value = palette.name;
  customHexInput.value = palette.colors.slice(0, 8).join(", ");
  applyCustomButton.textContent = "Update palette";
  customHexInput.focus();
  customHexInput.scrollIntoView({ behavior: "smooth", block: "center" });
}

function renderPalettes() {
  paletteContainer.innerHTML = palettes.map((palette, index) => {
    const swatches = Array.from({ length: 8 }, (_, swatchIndex) => {
      const color = palette.colors[swatchIndex] || palette.colors[0] || "#ffffff";
      return `<span class="palette_swatch" style="background:${color};" title="${color}"></span>`;
    }).join("");

    return `
      <article class="palette_card">
        <div class="palette_name_row">
          <h3>${palette.name}</h3>
          <div class="palette_card_actions">
            <button type="button" class="small_action edit_palette" data-index="${index}">Edit</button>
            <button type="button" class="small_action delete_palette" data-index="${index}">Delete</button>
          </div>
        </div>
        <div class="palette_swatches">${swatches}</div>
      </article>
    `;
  }).join("");
}

function addCustomPalette() {
  const customName = formatPaletteName(paletteNameInput.value);
  const parsedColors = parseCustomHexes(customHexInput.value);

  if (!parsedColors.length) {
    customHexInput.focus();
    customHexInput.setAttribute("aria-invalid", "true");
    customHexInput.placeholder = "Enter hex codes like #fff, #0f0, #ff00aa";
    return;
  }

  customHexInput.removeAttribute("aria-invalid");

  const fullPalette = {
    name: customName,
    colors: [...parsedColors.slice(0, 8)]
  };

  while (fullPalette.colors.length < 8) {
    fullPalette.colors.push(fullPalette.colors[fullPalette.colors.length - 1] || "#ffffff");
  }

  if (editingPaletteIndex !== null) {
    palettes[editingPaletteIndex] = fullPalette;
    resetCustomForm();
  } else {
    palettes = [fullPalette, ...palettes.filter((palette) => palette.name !== customName)];
  }

  renderPalettes();
}

if (paletteContainer) {
  paletteContainer.addEventListener("click", (event) => {
    const editButton = event.target.closest(".edit_palette");
    if (editButton) {
      const index = Number(editButton.dataset.index);
      preparePaletteForEditing(index);
      return;
    }

    const deleteButton = event.target.closest(".delete_palette");
    if (deleteButton) {
      const index = Number(deleteButton.dataset.index);
      if (Number.isInteger(index) && index >= 0 && index < palettes.length) {
        palettes.splice(index, 1);
        if (editingPaletteIndex === index || editingPaletteIndex === null) {
          resetCustomForm();
        }
        renderPalettes();
      }
    }
  });
}

if (applyCustomButton) {
  applyCustomButton.addEventListener("click", addCustomPalette);
}

if (paletteContainer && paletteNameInput && customHexInput && applyCustomButton) {
  loadDefaultPalettes();
}
