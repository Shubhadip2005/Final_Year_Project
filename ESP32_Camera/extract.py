import cv2
import urllib.request
import numpy as np
import pytesseract
import time
import platform

# --- CONFIGURATION ---
# 1. Update with your ESP32 IP
ESP32_IP = "10.22.235.166" 
STREAM_URL = f"http://{ESP32_IP}/"

# 2. Tesseract Path (Updated for your MacBook Air)
pytesseract.pytesseract.tesseract_cmd = r'C:\Program Files\Tesseract-OCR\tesseract.exe'

def extract_digital_reading(frame):
    """
    Processes the image to make digital segments readable.
    """
    # 1. Convert to Grayscale
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    
    # 2. Gaussian Blur to reduce sensor noise
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)
    
    # 3. Thresholding (Inverting to get Black text on White background)
    # Using Otsu's method to automatically calculate the best threshold
    _, thresh = cv2.threshold(blurred, 0, 255, cv2.THRESH_BINARY_INV + cv2.THRESH_OTSU)

    # 4. Tesseract Configuration
    # --psm 7: Treat the image as a single line of text
    # whitelist: Only look for numbers and decimal points
    custom_config = r'--psm 7 -c tessedit_char_whitelist=0123456789.'
    
    text = pytesseract.image_to_string(thresh, config=custom_config)
    return text.strip(), thresh

def main():
    print(f"Connecting to ESP32-CAM at {STREAM_URL}...")
    
    try:
        # Increase timeout for hotspot connections
        stream = urllib.request.urlopen(STREAM_URL, timeout=10)
    except Exception as e:
        print(f"Error: Connection failed. Ensure your Mac is on the same hotspot as the ESP32. \n{e}")
        return

    bytes_data = bytes()
    
    while True:
        try:
            bytes_data += stream.read(1024)
            a = bytes_data.find(b'\xff\xd8') # JPEG start
            b = bytes_data.find(b'\xff\xd9') # JPEG end
            
            if a != -1 and b != -1:
                jpg = bytes_data[a:b+2]
                bytes_data = bytes_data[b+2:]
                
                # Use np.uint8 for Python 3.13 compatibility
                frame = cv2.imdecode(np.frombuffer(jpg, dtype=np.uint8), cv2.IMREAD_COLOR)
                
                if frame is not None:
                    # RUN OCR
                    reading, debug_frame = extract_digital_reading(frame)
                    
                    if reading:
                        print(f"Current Reading: {reading} mA")
                    
                    # DISPLAY WINDOWS
                    cv2.imshow("ESP32-CAM Original", frame)
                    cv2.imshow("OCR Debug (Processed)", debug_frame)

        except Exception as e:
            print(f"Stream error: {e}")
            break

        # Press 'q' to exit
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()