document.addEventListener("click", (e) => {
  //  Modal Behavior
  if (e.target.matches("[data-modal-open]")) {
    const modal = document.querySelector(
      e.target.getAttribute("data-modal-open"),
    );
    modal?.classList.add("active");
  }
  if (e.target.matches("[data-modal-close], .modal")) {
    e.target.closest(".modal")?.classList.remove("active");
  }
});
