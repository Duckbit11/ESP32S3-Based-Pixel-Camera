document.addEventListener("DOMContentLoaded", () => {
    const galleryGrid = document.querySelector(".gallery_grid");
    const storageStrong = document.querySelector(".gallery_storage strong");
    const modal = document.getElementById("photo_modal");
    const modalImg = document.getElementById("modal_img");
    const modalFilename = document.getElementById("modal_filename");
    const downloadBtn = document.getElementById("download_btn");
    const closeModal = document.querySelector(".close_modal");

    // Fetch photos and storage info from ESP32 API
    fetch("/api/photos")
        .then(response => {
            if (!response.ok) throw new Error("Failed to load gallery");
            return response.json();
        })
        .then(data => {
            renderStorageInfo(data.used_mb, data.total_mb);
            renderGallery(data.photos);
        })
        .catch(err => {
            console.error("Gallery loading error:", err);
            galleryGrid.innerHTML = `<p class="gallery_empty">Failed to load photos from SD card.</p>`;
        });

    function renderStorageInfo(usedMB, totalMB) {
        if (!storageStrong) return;
        
        let used = (usedMB / 1024).toFixed(1);
        let total = Math.round(totalMB / 1024);
        
        if (total === 0) {
            storageStrong.textContent = `${usedMB} MB`;
        } else {
            storageStrong.textContent = `${used} / ${total} GB`;
        }
    }

    function renderGallery(photos) {
        if (!photos || photos.length === 0) {
            galleryGrid.innerHTML = `<p class="gallery_empty">No pixel photos captured yet.</p>`;
            return;
        }

        galleryGrid.innerHTML = "";

        // Reverse to show the newest photos first
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
        modalImg.src = url;
        modalFilename.textContent = filename;
        downloadBtn.href = url;
        downloadBtn.setAttribute("download", filename);
        modal.classList.add("active");
    }

    if (closeModal) {
        closeModal.addEventListener("click", () => modal.classList.remove("active"));
    }

    if (modal) {
        modal.addEventListener("click", (e) => {
            if (e.target === modal) modal.classList.remove("active");
        });
    }
});