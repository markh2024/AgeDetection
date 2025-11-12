const dropArea = document.getElementById('drop-area');
const fileInput = document.getElementById('photos');
const preview = document.getElementById('preview');

function preventDefaults(e) {
    e.preventDefault();
    e.stopPropagation();
}

['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
    dropArea.addEventListener(eventName, preventDefaults, false);
});

dropArea.addEventListener('dragover', () => dropArea.classList.add('highlight'));
dropArea.addEventListener('dragleave', () => dropArea.classList.remove('highlight'));
dropArea.addEventListener('drop', handleDrop);

function handleDrop(e) {
    dropArea.classList.remove('highlight');
    const dt = e.dataTransfer;
    const files = dt.files;
    fileInput.files = files;
    previewImages();
}

function previewImages() {
    preview.innerHTML = '';
    const files = fileInput.files;
    for (let i = 0; i < files.length; i++) {
        const img = document.createElement('img');
        img.src = URL.createObjectURL(files[i]);
        preview.appendChild(img);
    }
}

function showProcessing() {
    document.getElementById('processing').style.display = 'block';
}
