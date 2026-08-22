function populateFilterDropdown(grid_type, tags) {
  const select = document.getElementById(`${grid_type}Filter`);

  // Collect every unique ability across all tags
  const abilities = new Set();
  tags.forEach((tag) => {
    const builds =
      grid_type === "vehicle" ? [tag].concat(tag.rebuilds ?? []) : [tag];
    builds.forEach((b) => (b.abilities ?? []).forEach((a) => abilities.add(a)));
  });

  // Preserve current selection if it still exists after a reload
  const current = select.value;
  select.innerHTML = `<option value="">All Abilities</option>`;

  [...abilities]
    .filter((x) => x !== "")
    .sort()
    .forEach((ability) => {
      const opt = document.createElement("option");
      opt.value = ability;
      opt.textContent = ability;
      select.appendChild(opt);
    });

  if (current && abilities.has(current)) select.value = current;
}

function getFilteredTags(grid_type) {
  const select = document.getElementById(`${grid_type}Filter`);
  const chosen = select ? select.value : "";
  const tags = _allTags[grid_type];

  if (!chosen) return tags;

  return tags.filter((tag) => {
    const builds =
      grid_type === "vehicle" ? [tag].concat(tag.rebuilds ?? []) : [tag];
    return builds.some((b) => (b.abilities ?? []).includes(chosen));
  });
}

function updateGrid(grid_type, tags) {
  const grid = document.getElementById(`${grid_type}Grid`);
  grid.innerHTML = "";

  const sorted = [...tags].sort((a, b) => a.name.localeCompare(b.name));

  sorted.forEach((tag) => {
    const option = document.createElement("div");
    option.className = "character-option";

    if (grid_type === "character") {
      option.innerHTML = `
            <img src="${cdn}/${grid_type}s/${tag.id.toString().padStart(4, "0")}.webp"
                 alt="${tag.name}" ${cdnFallbackScript}>
            <p>${tag.name}</p>
        `;
      option.addEventListener("click", (evt) =>
        createNewToy(evt.target, tag, grid_type),
      );
    }

    if (grid_type === "vehicle") {
      const contents = [];
      for (const build of [tag].concat(tag.rebuilds ?? [])) {
        const el = document.createElement("div");
        el.className = "character-option";
        el.addEventListener("click", (evt) =>
          createNewToy(evt.target, build, grid_type),
        );
        el.innerHTML = `
                <img src="${cdn}/${grid_type}s/${build.id.toString().padStart(4, "0")}.webp"
                     alt="${build.name}" ${cdnFallbackScript}>
                <p>${build.name}</p>
            `;
        contents.push(el);
      }

      option.innerHTML = `
            <details>
              <summary style="display:flex;flex-direction:row;gap:1em;
                              justify-content:center;align-items:center">
                <img src="${cdn}/${grid_type}s/${tag.id.toString().padStart(4, "0")}.webp"
                     alt="${tag.name}" height="64" width="64" ${cdnFallbackScript}>
                <div>${tag.name}</div>
              </summary>
              <div class="tagList" style="display:flex;flex-direction:row;gap:1em;
                          justify-content:center;align-items:center;margin:20px 0">
              </div>
            </details>
        `;

      option.querySelector(".tagList").append(...contents);
    }

    grid.appendChild(option);
  });
}

function refreshGrid(grid_type) {
  updateGrid(grid_type, getFilteredTags(grid_type));
}

// Sends a new request to the server, creating a new toy for inside the toybox
async function createNewToy(element, tag, type) {
  await fetch("/toybox", {
    method: "PUT",
    headers: {
      "Content-Type": "application/x-www-form-urlencoded",
    },
    body: new URLSearchParams({
      name: tag.name,
      id: tag.id,
      type: type,
    }),
  }).then((x) => {
    updateToybox().then((x) => {
      // Close the modal
      element.closest(".modal")?.classList.remove("active");
    });
  });
}

// Wires up the character/vehicle filter dropdowns and loads the dimension data
function initGrid() {
  document
    .getElementById("characterFilter")
    .addEventListener("change", () => refreshGrid("character"));
  document
    .getElementById("vehicleFilter")
    .addEventListener("change", () => refreshGrid("vehicle"));

  fetch("/json/dimensions/characters.json").then(async (res) => {
    _allTags.character = await res.json();
    populateFilterDropdown("character", _allTags.character);
    refreshGrid("character");
  });

  fetch("/json/dimensions/vehicles.json").then(async (res) => {
    _allTags.vehicle = await res.json();
    populateFilterDropdown("vehicle", _allTags.vehicle);
    refreshGrid("vehicle");
  });
}
