<?php
$uploadDir = __DIR__ . '/uploads/';
$resultDir = __DIR__ . '/results/';
$maxWidth = 800;  // maximum width
$maxHeight = 800; // maximum height
$expiryTime = 3600; // 1 hour in seconds

if (!is_dir($uploadDir)) mkdir($uploadDir, 0755, true);
if (!is_dir($resultDir)) mkdir($resultDir, 0755, true);

$messages = [];
$results = [];

// -------------------------
// Auto-clean old files
// -------------------------
function cleanOldFiles($dir, $expiryTime) {
    $files = glob($dir . '*');
    $now = time();
    foreach ($files as $file) {
        if (is_file($file) && ($now - filemtime($file)) > $expiryTime) {
            unlink($file);
        }
    }
}

cleanOldFiles($uploadDir, $expiryTime);
cleanOldFiles($resultDir, $expiryTime);

// -------------------------
// Resize function
// -------------------------
function resizeImage($filePath, $maxWidth, $maxHeight) {
    $info = getimagesize($filePath);
    if (!$info) return false;

    list($width, $height) = $info;
    $mime = $info['mime'];

    switch ($mime) {
        case 'image/jpeg': $src = imagecreatefromjpeg($filePath); break;
        case 'image/png': $src = imagecreatefrompng($filePath); break;
        case 'image/gif': $src = imagecreatefromgif($filePath); break;
        default: return false;
    }

    $ratio = min($maxWidth / $width, $maxHeight / $height, 1);
    $newWidth = (int)($width * $ratio);
    $newHeight = (int)($height * $ratio);

    $dst = imagecreatetruecolor($newWidth, $newHeight);

    if ($mime == 'image/png' || $mime == 'image/gif') {
        imagecolortransparent($dst, imagecolorallocatealpha($dst, 0, 0, 0, 127));
        imagealphablending($dst, false);
        imagesavealpha($dst, true);
    }

    imagecopyresampled($dst, $src, 0, 0, 0, 0, $newWidth, $newHeight, $width, $height);

    switch ($mime) {
        case 'image/jpeg': imagejpeg($dst, $filePath, 90); break;
        case 'image/png': imagepng($dst, $filePath); break;
        case 'image/gif': imagegif($dst, $filePath); break;
    }

    imagedestroy($src);
    imagedestroy($dst);
    return true;
}

// -------------------------
// Handle uploads
// -------------------------
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_FILES['photos'])) {
    $files = $_FILES['photos'];
    $total = count($files['name']);

    for ($i = 0; $i < $total; $i++) {
        $fileName = basename($files['name'][$i]);
        $filePath = $uploadDir . $fileName;

        $allowedTypes = ['image/jpeg', 'image/png', 'image/bmp', 'image/gif'];
        if (!in_array($files['type'][$i], $allowedTypes)) {
            $messages[] = "Error: $fileName is not a valid image.";
            continue;
        }

        if (move_uploaded_file($files['tmp_name'][$i], $filePath)) {
            resizeImage($filePath, $maxWidth, $maxHeight);

            $outputFile = $resultDir . pathinfo($fileName, PATHINFO_FILENAME) . "_age_estimated.jpg";

            $binaryPath = escapeshellcmd(__DIR__ . "/age_estimator");
            $inputPath = escapeshellarg($filePath);
            $outputPath = escapeshellarg($outputFile);

            $cmd = "$binaryPath $inputPath $outputPath";
            exec($cmd, $output, $returnVar);

            if ($returnVar === 0 && file_exists($outputFile)) {
                $results[] = [
                    'original' => 'uploads/' . $fileName,
                    'age' => 'results/' . pathinfo($fileName, PATHINFO_FILENAME) . "_age_estimated.jpg"
                ];
            } else {
                $messages[] = "Error processing $fileName (code $returnVar).";
            }
        } else {
            $messages[] = "Failed to upload $fileName.";
        }
    }
}
?>

<!DOCTYPE html>
<html>
<head>
    <title>Age Detection</title>
    <link rel="stylesheet" href="style_age.css">
	<script src="age_detection.js"></script>
</head>
<body>
    <h1>Age Detection</h1>

    <?php foreach ($messages as $msg): ?>
        <p class="message"><?php echo htmlspecialchars($msg); ?></p>
    <?php endforeach; ?>

    <form method="post" enctype="multipart/form-data" onsubmit="showProcessing()">
    <div id="drop-area">
        <p>Drag & Drop images here or click to select</p>
        <input type="file" id="photos" name="photos[]" accept="image/*" multiple required onchange="previewImages()">
    </div>
    <div id="preview" class="preview"></div>
    <button type="submit">Upload & Detect Age</button>
	</form>

    <p id="processing" style="text-align:center; font-weight:bold; display:none;">Processing images, please wait...</p>

    <?php if (!empty($results)): ?>
        <div class="gallery">
            <?php foreach ($results as $res): ?>
                <div class="card">
                    <strong>Original</strong>
                    <img src="<?php echo htmlspecialchars($res['original']); ?>" alt="Original">
                    <strong>Age Estimated</strong>
                    <img src="<?php echo htmlspecialchars($res['age']); ?>" alt="Age Estimated">
                </div>
            <?php endforeach; ?>
        </div>
    <?php endif; ?>
</body>
</html>
