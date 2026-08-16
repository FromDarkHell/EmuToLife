// Handle pad clicks for toy movement
function toyClick(evt) {
  const container = evt.target.closest(".toy-item");
  const toyUid = container.getAttribute("data-uid");

  // If we're selecting the same toy that's already selected, we probably wanna just de-select it.
  if (selectedToy !== null) {
    if (selectedToy.uid == toyUid) {
      clearSelection();
      return;
    }
  }

  clearSelection();
  selectedToy = toybox.find((x) => x.uid == toyUid);

  container.classList.add("selected");
  console.log(`Selected toy: ${JSON.stringify(selectedToy)}`);
}

function swapToyWithPad(selectedToy, targetPadId) {
  const toyOnTargetPad = toypadToys[targetPadId];

  // If selected toy is from toybox
  if (selectedToy.currentPadId === undefined) {
    // Move toy from toybox to pad
    toypadToys[targetPadId] = selectedToy;
    // (toyOnTargetPad will automatically appear in toybox when renderToybox() runs)
  }
  // If selected toy is from another pad
  else {
    // Clear the source pad
    delete toypadToys[selectedToy.currentPadId];
    // Put selected toy on target pad
    toypadToys[targetPadId] = selectedToy;
    // Put displaced toy on source pad
    toypadToys[selectedToy.currentPadId] = toyOnTargetPad;
  }

  // Re-render affected areas
  if (selectedToy.currentPadId !== undefined) {
    renderPad(selectedToy.currentPadId);
  }
  renderPad(targetPadId);
  renderToybox();

  console.log(
    `Swapped toys: ${selectedToy.name} moved to pad ${targetPadId}, ${
      toyOnTargetPad.name
    } ${
      selectedToy.currentPadId !== undefined
        ? `moved to pad ${selectedToy.currentPadId}`
        : "moved to toybox"
    }`,
  );
}

function swapToysBetweenPads(sourcePadId, targetPadId) {
  const sourceToy = toypadToys[sourcePadId];
  const targetToy = toypadToys[targetPadId];

  // Swap the toys
  toypadToys[sourcePadId] = targetToy;
  toypadToys[targetPadId] = sourceToy;

  // Re-render both pads
  renderPad(sourcePadId);
  renderPad(targetPadId);

  console.log(
    `Swapped toys: ${sourceToy.name} and ${targetToy.name} between pads ${sourcePadId} and ${targetPadId}`,
  );
}

function handleToyOnPadClick(e, padId) {
  e.stopPropagation();

  const toyOnPad = toypadToys[padId];
  if (!toyOnPad) return;

  // Clicking the already-selected toy on its pad
  if (
    selectedToy &&
    selectedToy.uid === toyOnPad.uid &&
    selectedToy.currentPadId === padId
  ) {
    clearSelection();
    return;
  }

  // No selection yet, just select this pad toy
  if (!selectedToy) {
    selectedToy = { ...toyOnPad, currentPadId: padId };
    renderPad(padId); // re-render to show action overlay
    return;
  }

  // A different toy is selected, swap or place
  if (selectedToy.currentPadId === undefined) {
    swapToyWithPad(selectedToy, padId);
  } else {
    swapToysBetweenPads(selectedToy.currentPadId, padId);
  }

  clearSelection();
}

function moveToyToPad(toy, padId, updateToybox = true) {
  console.log(`Moved toy ${toy.name} to pad ${padId}`);

  toypadToys[padId] = toy;
  renderPad(padId);

  if (updateToybox) {
    renderToybox();
  }
}

function moveToyBetweenPads(fromPadId, toPadId) {
  const toy = toypadToys[fromPadId];
  delete toypadToys[fromPadId];
  toypadToys[toPadId] = toy;

  renderPad(fromPadId);
  renderPad(toPadId);
  console.log(`Moved toy ${toy.name} from pad ${fromPadId} to pad ${toPadId}`);
}

function moveToyToToybox(padId) {
  const toy = toypadToys[padId];
  toypadToys[padId] = undefined;
  delete toypadToys[padId];

  renderPad(padId);
  renderToybox();
  console.log(`Moved toy ${toy.name} back to toybox`);
}
