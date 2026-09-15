document.addEventListener("DOMContentLoaded", () => {
    const galleryGrid = document.querySelector(".gallery_grid");
    const storageStrong = document.querySelector(".gallery_storage strong");
    const modal = document.getElementById("photo_modal");
    const modalImg = document.getElementById("modal_img");
    const modalFilename = document.getElementById("modal_filename");
    const downloadBtn = document.getElementById("download_btn");
    const closeModal = document.querySelector(".close_modal");
    const deleteBtn = document.getElementById("delete_btn");
    let currentPhotoName = null;

    async function loadGallery() {
        try {
            const response = await fetch("/api/photos");
            if (!response.ok) throw new Error("Failed to load gallery");

            const data = await response.json();
            renderStorageInfo(data.used_mb, data.total_mb);
            renderGallery(data.photos);
        } catch (err) {
            console.error("Gallery loading error:", err);
            if (galleryGrid) {
                galleryGrid.innerHTML = `<p class="gallery_empty">Failed to load photos from SD card.</p>`;
            }
        }
    }

    function renderStorageInfo(usedMB, totalMB) {
        if (!storageStrong) return;

        const used = Number(usedMB) || 0;
        const total = Number(totalMB) || 0;
        const usedGB = (used / 1024).toFixed(1);
        const totalGB = Math.round(total / 1024);

        if (total === 0) {
            storageStrong.textContent = `${used} MB`;
        } else {
            storageStrong.textContent = `${usedGB} / ${totalGB} GB`;
        }
    }

    function renderGallery(photos) {
        if (!galleryGrid) return;

        if (!photos || photos.length === 0) {
            galleryGrid.innerHTML = `<p class="gallery_empty">No pixel photos captured yet.</p>`;
            return;
        }

        galleryGrid.innerHTML = "";

        photos.slice().reverse().forEach(filename => {
            const fileUrl = `/file?name=${encodeURIComponent(filename)}`;

            const card = document.createElement("div");
            card.className = "gallery_card";
            card.innerHTML = `
                <div class="img_wrapper">
                    <img src="${fileUrl}" alt="${filename}" loading="lazy" />
                </div>
                <p class="filename_label">${filename}</p>
            `;

            card.addEventListener("click", () => openLightbox(fileUrl, filename));
            galleryGrid.appendChild(card);
        });
    }

    function openLightbox(url, filename) {
        currentPhotoName = filename;

        if (modalImg) modalImg.src = url;
        if (modalFilename) modalFilename.textContent = filename;
        if (downloadBtn) {
            downloadBtn.href = url;
            downloadBtn.setAttribute("download", filename);
        }
        if (modal) modal.classList.add("active");
    }

    function closeLightbox() {
        if (modal) modal.classList.remove("active");
        currentPhotoName = null;
    }

    async function deleteCurrentPhoto() {
        if (!currentPhotoName) return;

        // Prevent accidental clicks
        const confirmed = confirm(`Are you sure you want to delete "${currentPhotoName}"?`);
        if (!confirmed) return;

        try {
            if (deleteBtn) deleteBtn.disabled = true;

            // Single targeted endpoint request
            const response = await fetch(`/api/photos?name=${encodeURIComponent(currentPhotoName)}`, {
                method: "DELETE"
            });

            if (!response.ok) {
                throw new Error(`Delete failed with status ${response.status}`);
            }

            closeLightbox();
            await loadGallery();
        } catch (error) {
            console.error("Photo delete failed:", error);
            alert("Unable to delete the selected photo. Please try again.");
        } finally {
            if (deleteBtn) deleteBtn.disabled = false;
        }
    }

    if (closeModal) {
        closeModal.addEventListener("click", closeLightbox);
    }

    if (modal) {
        modal.addEventListener("click", (e) => {
            if (e.target === modal) closeLightbox();
        });
    }

    if (deleteBtn) {
        deleteBtn.addEventListener("click", deleteCurrentPhoto);
    }

    loadGallery();
});