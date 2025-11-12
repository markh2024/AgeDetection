#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <iostream>
#include <vector>
#include <filesystem>

using namespace cv;
using namespace cv::dnn;
using namespace std;
namespace fs = std::filesystem;

// Age categories used by the pretrained model
const vector<string> AGE_LIST = {
    "(0-2)", "(4-6)", "(8-12)", "(15-20)",
    "(25-32)", "(38-43)", "(48-53)", "(60-100)"
};

// Configuration
const float FACE_CONFIDENCE_THRESHOLD = 0.5;
const int FACE_DETECTION_SIZE = 300;
const int AGE_INPUT_SIZE = 227;
const Scalar FACE_MEAN(104.0, 177.0, 123.0);
const Scalar AGE_MEAN(78.4263377603, 87.7689143744, 114.895847746);
const Scalar BOX_COLOR(0, 255, 0); // Green
const Scalar TEXT_BG_COLOR(0, 255, 0); // Green background
const Scalar TEXT_COLOR(0, 0, 0); // Black text

// Helper function to find model files
string findModelFile(const string& filename) {
    vector<string> searchPaths = {
        "./models/",
        "../models/",
        "/home/mark/public_html/age_web/models/",
        "./"
    };
    
    for (const auto& path : searchPaths) {
        string fullPath = path + filename;
        if (fs::exists(fullPath)) {
            return fullPath;
        }
    }
    
    return "";
}

int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <input_image> <output_image>" << endl;
        cerr << "Example: " << argv[0] << " photo.jpg output.jpg" << endl;
        return 1;
    }

    string inputPath = argv[1];
    string outputPath = argv[2];

    // Load image
    Mat img = imread(inputPath);
    if (img.empty()) {
        cerr << "Error: Could not read image: " << inputPath << endl;
        return 1;
    }

    // Find and load DNN models
    string faceProto = findModelFile("deploy.prototxt");
    string faceModel = findModelFile("res10_300x300_ssd_iter_140000.caffemodel");
    string ageProto = findModelFile("deploy_age.prototxt");
    string ageModel = findModelFile("age_net.caffemodel");

    if (faceProto.empty() || faceModel.empty()) {
        cerr << "Error: Face detection model files not found." << endl;
        cerr << "Looking for: deploy.prototxt and res10_300x300_ssd_iter_140000.caffemodel" << endl;
        return 1;
    }

    if (ageProto.empty() || ageModel.empty()) {
        cerr << "Error: Age estimation model files not found." << endl;
        cerr << "Looking for: deploy_age.prototxt and age_net.caffemodel" << endl;
        return 1;
    }

    cout << "Loading models..." << endl;
    cout << "Face detection: " << faceModel << endl;
    cout << "Age estimation: " << ageModel << endl;

    Net faceNet, ageNet;
    
    try {
        faceNet = readNetFromCaffe(faceProto, faceModel);
        ageNet = readNetFromCaffe(ageProto, ageModel);
    } catch (const cv::Exception& e) {
        cerr << "Error loading models: " << e.what() << endl;
        return 1;
    }

    // Check if models loaded successfully
    if (faceNet.empty() || ageNet.empty()) {
        cerr << "Error: Failed to load neural networks." << endl;
        return 1;
    }

    // Set backend to CPU (more compatible)
    faceNet.setPreferableBackend(DNN_BACKEND_OPENCV);
    faceNet.setPreferableTarget(DNN_TARGET_CPU);
    ageNet.setPreferableBackend(DNN_BACKEND_OPENCV);
    ageNet.setPreferableTarget(DNN_TARGET_CPU);

    int h = img.rows;
    int w = img.cols;

    cout << "Processing image (" << w << "x" << h << ")..." << endl;

    // Face detection
    Mat blob = blobFromImage(img, 1.0, Size(FACE_DETECTION_SIZE, FACE_DETECTION_SIZE), 
                             FACE_MEAN, false, false);
    faceNet.setInput(blob);
    Mat detections = faceNet.forward();

    // Convert 4D detections blob to 2D matrix
    Mat detectionMat(detections.size[2], detections.size[3], CV_32F, detections.ptr<float>());

    int facesDetected = 0;

    for (int i = 0; i < detectionMat.rows; i++) {
        float confidence = detectionMat.at<float>(i, 2);
        
        if (confidence > FACE_CONFIDENCE_THRESHOLD) {
            // Get bounding box coordinates
            int x1 = static_cast<int>(detectionMat.at<float>(i, 3) * w);
            int y1 = static_cast<int>(detectionMat.at<float>(i, 4) * h);
            int x2 = static_cast<int>(detectionMat.at<float>(i, 5) * w);
            int y2 = static_cast<int>(detectionMat.at<float>(i, 6) * h);

            // Ensure bounding box is within image bounds
            x1 = max(0, x1);
            y1 = max(0, y1);
            x2 = min(w, x2);
            y2 = min(h, y2);

            // Skip invalid boxes
            if (x2 <= x1 || y2 <= y1) continue;

            Rect faceRect(Point(x1, y1), Point(x2, y2));
            
            try {
                Mat face = img(faceRect);
                
                // Age estimation
                Mat faceBlob = blobFromImage(face, 1.0, Size(AGE_INPUT_SIZE, AGE_INPUT_SIZE),
                                             AGE_MEAN, false);

                ageNet.setInput(faceBlob);
                Mat agePreds = ageNet.forward();
                
                Point classIdPoint;
                double confidenceAge;
                minMaxLoc(agePreds, nullptr, &confidenceAge, nullptr, &classIdPoint);
                int ageIdx = classIdPoint.x;

                if (ageIdx >= 0 && ageIdx < static_cast<int>(AGE_LIST.size())) {
                    string label = AGE_LIST[ageIdx];
                    
                    // Calculate text size for background
                    int baseline = 0;
                    Size textSize = getTextSize(label, FONT_HERSHEY_SIMPLEX, 0.8, 2, &baseline);
                    
                    // Draw text background
                    int labelY = max(y1 - 10, textSize.height + 5);
                    rectangle(img, 
                             Point(x1, labelY - textSize.height - 5),
                             Point(x1 + textSize.width + 10, labelY + 5),
                             TEXT_BG_COLOR, FILLED);
                    
                    // Draw text
                    putText(img, label, Point(x1 + 5, labelY), 
                           FONT_HERSHEY_SIMPLEX, 0.8, TEXT_COLOR, 2);
                    
                    // Draw bounding box
                    rectangle(img, faceRect, BOX_COLOR, 2);
                    
                    facesDetected++;
                    cout << "Face " << facesDetected << ": " << label 
                         << " (confidence: " << fixed << setprecision(2) << confidence << ")" << endl;
                }
            } catch (const cv::Exception& e) {
                cerr << "Warning: Error processing face: " << e.what() << endl;
                continue;
            }
        }
    }

    if (facesDetected == 0) {
        cout << "No faces detected in the image." << endl;
        // Add watermark to indicate no faces found
        string noFaceText = "No faces detected";
        int baseline = 0;
        Size textSize = getTextSize(noFaceText, FONT_HERSHEY_SIMPLEX, 1.0, 2, &baseline);
        Point textOrg((img.cols - textSize.width) / 2, (img.rows + textSize.height) / 2);
        
        rectangle(img, 
                 Point(textOrg.x - 10, textOrg.y - textSize.height - 10),
                 Point(textOrg.x + textSize.width + 10, textOrg.y + 10),
                 Scalar(0, 0, 0), FILLED);
        putText(img, noFaceText, textOrg, FONT_HERSHEY_SIMPLEX, 1.0, Scalar(255, 255, 255), 2);
    } else {
        cout << "Total faces detected: " << facesDetected << endl;
    }

    // Save annotated image
    vector<int> compression_params;
    compression_params.push_back(IMWRITE_JPEG_QUALITY);
    compression_params.push_back(95); // High quality
    
    if (!imwrite(outputPath, img, compression_params)) {
        cerr << "Error: Could not write output image: " << outputPath << endl;
        return 1;
    }

    cout << "Success! Output saved to: " << outputPath << endl;
    return 0;
}
