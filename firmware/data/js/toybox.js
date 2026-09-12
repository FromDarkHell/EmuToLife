// Deletes the currently selected toy from the toybox / pad
async function deleteSelected(element) {
  if (selectedToy == null) return;

  if (selectedToy.currentPadId !== undefined) {
    moveToyToToybox(selectedToy.currentPadId);
  }

  await fetch("/toybox-delete", {
    method: "POST",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded",
    },
    body: new URLSearchParams({
      uid: selectedToy.uid,
    }),
  }).then((x) => {
    updateToybox();
    clearSelection();
  });
}

function clearSelection() {
  document
    .querySelectorAll(".toy-item.selected")
    .forEach((el) => el.classList.remove("selected"));
  document
    .querySelectorAll(".toy-on-pad.selected")
    .forEach((el) => el.classList.remove("selected"));

  // Re-render any pad that was showing action buttons
  if (selectedToy !== null && selectedToy.currentPadId !== undefined) {
    const currentPad = selectedToy.currentPadId;
    selectedToy = null;
    renderPad(currentPad);
  } else {
    selectedToy = null;
  }
}

async function updateToybox() {
  await fetch("/toybox").then(async (data) => {
    toybox = await data.json();
    console.log(`Loaded toybox data...`);
    renderToybox(true);
  });
}

function renderToybox(updatePads = false) {
  console.log("Updating toybox...");

  const grid = document.getElementById(`toybox`);
  grid.innerHTML = "";

  if (updatePads) {
    const previousSlotByUid = {};
    Object.entries(_toypadToys).forEach(([slot, toy]) => {
      if (toy) previousSlotByUid[toy.uid] = slot;
    });

    Object.keys(_toypadToys).forEach((padId) => delete _toypadToys[padId]);

    // Both games' pad grids reuse the same small data-pad-index numbers (see
    // activePadGridId()'s comment), so a placed toy from the *other* game must never be rendered
    // onto the active grid - it'd land on an unrelated, colliding pad slot.
    const toysOnPad = toybox.filter(
      (toy) => toy.index != -1 && toy.game === selectedGame,
    );
    console.log(`Need to place ${JSON.stringify(toysOnPad)} onto toypad`);

    const occupiedSlots = new Set();
    const unassignedToys = [];

    // Keep a toy in whatever slot it already occupied, if that slot is still valid for its
    // (possibly unchanged) location - only newly-appearing toys get assigned a fresh slot below.
    // Otherwise every refresh would re-sort a whole stack by array order instead of leaving
    // already-placed toys where they are.
    toysOnPad.forEach((toy) => {
      const slots = slotsForLocation(toy.index);
      const previousSlot = previousSlotByUid[toy.uid];

      if (previousSlot && slots.includes(previousSlot)) {
        _toypadToys[previousSlot] = toy;
        occupiedSlots.add(previousSlot);
      } else {
        unassignedToys.push(toy);
      }
    });

    unassignedToys.forEach((toy) => {
      const freeSlot = slotsForLocation(toy.index).find(
        (slot) => !occupiedSlots.has(slot),
      );
      if (!freeSlot) return;

      _toypadToys[freeSlot] = toy;
      occupiedSlots.add(freeSlot);
    });

    // Re-render every slot in the active grid, not just occupied ones, so a slot that lost its
    // toy in this refresh (a stack shrinking) gets cleared instead of showing stale content.
    document
      .querySelectorAll(`#${activePadGridId()} .pad[data-pad-index]`)
      .forEach((el) => renderPad(el.getAttribute("data-pad-index")));
  }

  // Only show toys for the active game that aren't already on the toypad - a toy from the other
  // game can't be placed on this grid anyway (the firmware rejects it), so hide it here too.
  const availableToys = toybox.filter(
    (toy) =>
      toy.game === selectedGame &&
      !Object.values(toypadToys).some((padToy) => padToy.uid === toy.uid),
  );

  console.log(`Available Toybox Toys: ${JSON.stringify(availableToys)}`);

  availableToys.forEach((toy) => {
    const item = document.createElement("div");
    item.className = "toy-item";

    item.setAttribute("data-type", toy.type);
    item.setAttribute("data-uid", toy.uid);
    item.setAttribute("data-id", toy.id);

    item.innerHTML = `
                          <img src="${cdn}/${imageFolderForType(toy.type)}/${toy.id
                            .toString()
                            .padStart(
                              4,
                              "0",
                            )}.webp" alt="${toy.name}" height="80" width="80" ${cdnFallbackScript}>
                  <p>${toy.name}</p>
    `;

    item.addEventListener("click", toyClick);
    grid.appendChild(item);
  });
}

function renderPad(padId) {
  console.log(`Rendering pad: ${padId}`);
  // Scoped to the active game's grid - both grids reuse the same small data-pad-index numbers.
  const pad = document.querySelector(
    `#${activePadGridId()} [data-pad-index="${padId}"]`,
  );
  const toy = toypadToys[padId];

  if (!toy) {
    pad.classList.remove("occupied");
    pad.innerHTML = "";
    return;
  }

  const isSelected =
    selectedToy &&
    selectedToy.uid === toy.uid &&
    selectedToy.currentPadId == padId;

  pad.classList.add("occupied");
  pad.innerHTML = `
    <div class="toy-on-pad ${isSelected ? "selected" : ""}" data-uid="${toy.uid}">
        <img src="${cdn}/${imageFolderForType(toy.type)}/${toy.id.toString().padStart(4, "0")}.webp"
             alt="${toy.name}" class="toy-on-pad-image" ${cdnFallbackScript}>
        <p>${toy.name}</p>
        ${
          isSelected
            ? `
        <div class="pad-toy-actions">
            <button class="pad-toy-action action-putback" title="Put back">Place</button>
            <button class="pad-toy-action action-remove"  title="Remove">Remove</button>
        </div>`
            : ""
        }
    </div>
`;

  // Wire up the action buttons if present
  if (isSelected) {
    pad.querySelector(".action-putback").addEventListener("click", (e) => {
      e.stopPropagation();
      moveToyToPad(selectedToy, selectedToy.currentPadId);
      clearSelection(); // already in correct place, just deselect
    });

    pad.querySelector(".action-remove").addEventListener("click", (e) => {
      e.stopPropagation();
      moveToyToToybox(padId);
      clearSelection();
    });
  }
}
