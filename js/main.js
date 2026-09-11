document.addEventListener("DOMContentLoaded", () => {
    const storageStrong = document.getElementById("storage_used_percent");

    if (!storageStrong) return;

    fetch("/api/photos")
        .then(response => {
            if (!response.ok) throw new Error("Failed to load storage data");
            return response.json();
        })
        .then(data => {
            const usedMB = Number(data.used_mb) || 0;
            const totalMB = Number(data.total_mb) || 0;
            const percentage = totalMB > 0 ? Math.min(Math.round((usedMB / totalMB) * 100), 100) : 0;

            storageStrong.textContent = `${percentage}%`;
        })
        .catch(err => {
            console.error("Storage loading error:", err);
            storageStrong.textContent = "0%";
        });
});
