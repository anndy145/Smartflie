$ModelDir = "models"
if (!(Test-Path $ModelDir)) { New-Item -ItemType Directory -Path $ModelDir | Out-Null }

$BaseUrl = "https://huggingface.co/Xenova/clip-vit-base-patch32/resolve/main/onnx"
$VisionModel = "vision_model.onnx"
$TextModel = "text_model.onnx"

Write-Host "Downloading CLIP Vision Model..."
Invoke-WebRequest -Uri "$BaseUrl/$VisionModel" -OutFile "$ModelDir/clip-vision.onnx"
Write-Host "Vision Model Downloaded to $ModelDir/clip-vision.onnx"

Write-Host "Downloading CLIP Text Model..."
Invoke-WebRequest -Uri "$BaseUrl/$TextModel" -OutFile "$ModelDir/clip-text.onnx"
Write-Host "Text Model Downloaded to $ModelDir/clip-text.onnx"
