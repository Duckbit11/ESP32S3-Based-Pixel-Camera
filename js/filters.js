const paletteContainer = document.getElementById("palette_container");
const paletteNameInput = document.getElementById("palette_name");
const customHexInput = document.getElementById("custom_hex_input");
const applyCustomButton = document.getElementById("apply_custom_palette");

let palettes = [];
let editingPaletteIndex = null;

async function fetchPalettes() {
  const response = await fetch("/api/palettes");
  if (!response.ok) {
    throw new Error("Unable to load palette data");
  }

  const data = await response.json();
  palettes = Array.isArray(data.palettes) ? data.palettes : [];
  renderPalettes();
}

async function savePalettes() {
  const response = await fetch("/api/palettes", {
    method: "PUT",
    headers: {
      "Content-Type": "application/json",
    },
    body: JSON.stringify({ palettes }),
  });

  if (!response.ok) {
    throw new Error("Unable to save palette data");
  }

  await fetchPalettes();
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

  return tokens
    .map((token) => normalizeHex(token))
    .filter(Boolean);
}

function resetCustomForm() {
  editingPaletteIndex = null;
  applyCustomButton.textContent = "Apply custom palette";
  paletteNameInput.value = "My Custom Palette";
  customHexInput.value = "";
}

function preparePaletteForEditing(index) {
  const palette = palettes[index];
  if (!palette) return;

  editingPaletteIndex = index;
  paletteNameInput.value = palette.name;
  customHexInput.value = Array.isArray(palette.colors) ? palette.colors.slice(0, 16).join(", ") : "";
  applyCustomButton.textContent = "Update palette";
  customHexInput.focus();
  customHexInput.scrollIntoView({ behavior: "smooth", block: "center" });
}

function renderPalettes() {
  if (!paletteContainer) return;

  paletteContainer.innerHTML = palettes.map((palette, index) => {
    const swatches = Array.from({ length: 16 }, (_, swatchIndex) => {
      const color = palette.colors?.[swatchIndex] || palette.colors?.[0] || "#ffffff";
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

async function addCustomPalette() {
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
    colors: [...parsedColors.slice(0, 16)]
  };

  while (fullPalette.colors.length < 16) {
    const fallback = fullPalette.colors[fullPalette.colors.length - 1] || "#ffffff";
    fullPalette.colors.push(fallback);
  }

  const updatedPalettes = [...palettes];
  if (editingPaletteIndex !== null) {
    updatedPalettes[editingPaletteIndex] = fullPalette;
  } else {
    updatedPalettes.unshift(fullPalette);
  }

  palettes = updatedPalettes;
  renderPalettes();

  try {
    await savePalettes();
    resetCustomForm();
  } catch (error) {
    console.error("Failed to save palette:", error);
    alert("Saving failed. Please try again.");
  }
}

async function deletePalette(index) {
  if (!Number.isInteger(index) || index < 0 || index >= palettes.length) {
    return;
  }

  const copy = [...palettes];
  copy.splice(index, 1);
  palettes = copy;
  renderPalettes();

  if (editingPaletteIndex === index) {
    resetCustomForm();
  }

  try {
    await savePalettes();
  } catch (error) {
    console.error("Failed to delete palette:", error);
    alert("Delete failed. Please try again.");
  }
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
      deletePalette(index);
    }
  });
}

if (applyCustomButton) {
  applyCustomButton.addEventListener("click", addCustomPalette);
}

if (paletteContainer && paletteNameInput && customHexInput && applyCustomButton) {
  fetchPalettes().catch((error) => {
    console.error("Failed to load palettes:", error);
    palettes = [];
    renderPalettes();
  });
}
