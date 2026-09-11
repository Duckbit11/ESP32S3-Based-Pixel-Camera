document.addEventListener("DOMContentLoaded", () => {
    const totalPhotosStrong = document.getElementById("total_photos_taken");
    const storageStrong = document.getElementById("storage_used_percent");

    fetch("/api/photos")
        .then(response => {
            if (!response.ok) throw new Error("Failed to load storage data");
            return response.json();
        })
        .then(data => {
            const photos = Array.isArray(data.photos) ? data.photos : [];
            const totalPhotos = photos.length;
            const usedMB = Number(data.used_mb) || 0;
            const totalMB = Number(data.total_mb) || 0;
            const percentage = totalMB > 0 ? Math.min(Math.round((usedMB / totalMB) * 100), 100) : 0;

            if (totalPhotosStrong) {
                totalPhotosStrong.textContent = totalPhotos;
            }

            if (storageStrong) {
                storageStrong.textContent = `${percentage}%`;
            }
        })
        .catch(err => {
            console.error("Storage loading error:", err);

            if (totalPhotosStrong) {
                totalPhotosStrong.textContent = "0";
            }

            if (storageStrong) {
                storageStrong.textContent = "0%";
            }
        });
});
