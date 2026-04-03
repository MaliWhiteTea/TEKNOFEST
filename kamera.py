import cv2
import numpy as np
import math
import threading
import csv
from datetime import datetime
import smbus
import time
from gpiozero import Button
from signal import pause

# ---------- ARDUINO I2C AYARLARI ----------
bus = smbus.SMBus(1)        # I2C bus (Raspberry Pi’de genelde 1)
ARDUINO_ADDR = 0x08         # Arduino I2C adresi

def send_text(text):
    """Arduino'ya stringi tek seferde gönder"""
    data = [ord(c) for c in text]        # string → ASCII listesi
    try:
        bus.write_i2c_block_data(ARDUINO_ADDR, 0, data)  # tek seferde gönder
    except Exception as e:
        print("Arduino'ya veri gönderilemedi:", e)


# ---------- KAMERA ve SAYIM AYARLARI ----------
def euclidean_distance(p1, p2):
    return math.hypot(p1[0] - p2[0], p1[1] - p2[1])

video_source = 0  # USB webcam

area_threshold = 2000
line_thickness = 2
min_distance_for_tracking = 60
timeout_frames = 50

margin_top = 50
margin_bottom = 50
margin_left = 40
margin_right = 60

counts = {"top": 0, "bottom": 0, "left": 0, "right": 0}

tracked_objects = []
next_object_id = 0
frame_index = 0

cap = cv2.VideoCapture(video_source)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

if not cap.isOpened():
    print("USB Kamera açılamadı!")
    exit()

bg_subtractor = cv2.createBackgroundSubtractorMOG2(history=100, varThreshold=25, detectShadows=True)

def get_line_positions(height, width):
    return {
        "top": margin_top,
        "bottom": height - margin_bottom,
        "left": margin_left,
        "right": width - margin_right
    }

# CSV dosyası oluşturma
csv_file = "sayim_verileri.csv"
with open(csv_file, mode='w', newline='') as file:
    writer = csv.writer(file)
    writer.writerow(["Zaman", "Ust", "Alt", "Sol", "Sag"])


def pressed():
    global counts
    counts = {"top":0, "bottom":0, "left":0, "right":0}
     

button = Button(22, pull_up=False)
button.when_pressed = pressed
# Terminalden sıfırlama 
def reset_listener():
    global counts
    while True:
        cmd = input()
        if cmd.lower() == "reset":
            counts = {"top":0, "bottom":0, "left":0, "right":0}
            print("Sayımlar sıfırlandı terminalden!")

listener_thread = threading.Thread(target=reset_listener, daemon=True)
listener_thread.start()

def save_to_csv():
    with open(csv_file, mode='a', newline='') as file:
        writer = csv.writer(file)
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        writer.writerow([now, counts["top"], counts["bottom"], counts["left"], counts["right"]])

# ---------- ANA DÖNGÜ ----------
while True:
    ret, frame = cap.read()
    if not ret:
        print("Kamera görüntüsü alınamadı.")
        break

    frame = cv2.flip(frame, 1)
    frame_index += 1
    height, width, _ = frame.shape
    line_positions = get_line_positions(height, width)

    fg_mask = bg_subtractor.apply(frame)
    fg_mask[fg_mask == 127] = 0

    kernel = np.ones((5, 5), np.uint8)
    opened = cv2.morphologyEx(fg_mask, cv2.MORPH_OPEN, kernel, iterations=2)
    dilated = cv2.dilate(opened, kernel, iterations=2)

    contours, _ = cv2.findContours(dilated, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    detections = []

    centers, bboxes = [], []
    for cnt in contours:
        area = cv2.contourArea(cnt)
        if area > area_threshold:
            x, y, w, h = cv2.boundingRect(cnt)
            cx, cy = x + w // 2, y + h // 2
            aspect_ratio = w / h
            if 0.5 < aspect_ratio < 4.0:
                centers.append((cx, cy))
                bboxes.append((x, y, w, h))

    used = set()
    merge_distance = 40
    for i, c1 in enumerate(centers):
        if i in used:
            continue
        group = [i]
        for j, c2 in enumerate(centers):
            if i != j and j not in used:
                if euclidean_distance(c1, c2) < merge_distance:
                    group.append(j)
        xs = [bboxes[k][0] for k in group]
        ys = [bboxes[k][1] for k in group]
        ws = [bboxes[k][0] + bboxes[k][2] for k in group]
        hs = [bboxes[k][1] + bboxes[k][3] for k in group]
        x, y = min(xs), min(ys)
        w, h = max(ws) - x, max(hs) - y
        cx, cy = x + w // 2, y + h // 2
        detections.append({'center': (cx, cy), 'bbox': (x, y, w, h)})
        for k in group:
            used.add(k)
        cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
        cv2.circle(frame, (cx, cy), 5, (0, 0, 255), -1)

    new_tracked = []
    for det in detections:
        matched = False
        for obj in tracked_objects:
            if euclidean_distance(det['center'], obj['center']) < min_distance_for_tracking:
                obj['previous_center'] = obj['center']
                obj['center'] = det['center']
                obj['last_seen_frame'] = frame_index

                changed = False
                if "top" not in obj['crossed_lines']:
                    if obj['previous_center'][1] < line_positions["top"] and obj['center'][1] >= line_positions["top"]:
                        counts["top"] += 1
                        obj['crossed_lines'].add("top")
                        changed = True

                if "bottom" not in obj['crossed_lines']:
                    if obj['previous_center'][1] > line_positions["bottom"] and obj['center'][1] <= line_positions["bottom"]:
                        counts["bottom"] += 1
                        obj['crossed_lines'].add("bottom")
                        changed = True

                if "left" not in obj['crossed_lines']:
                    if obj['previous_center'][0] < line_positions["left"] and obj['center'][0] >= line_positions["left"]:
                        counts["left"] += 1
                        obj['crossed_lines'].add("left")
                        changed = True

                if "right" not in obj['crossed_lines']:
                    if obj['previous_center'][0] > line_positions["right"] and obj['center'][0] <= line_positions["right"]:
                        counts["right"] += 1
                        obj['crossed_lines'].add("right")
                        changed = True

                if changed:
                    save_to_csv()

                new_tracked.append(obj)
                matched = True
                break

        if not matched:
            new_obj = {
                'id': next_object_id,
                'center': det['center'],
                'previous_center': det['center'],
                'crossed_lines': set(),
                'last_seen_frame': frame_index
            }
            new_tracked.append(new_obj)
            next_object_id += 1

    tracked_objects = [obj for obj in new_tracked if (frame_index - obj['last_seen_frame']) < timeout_frames]

    # Çizgiler
    top = max(0, min(line_positions["top"], height-1))
    bottom = max(0, min(line_positions["bottom"], height-1))
    left = max(0, min(line_positions["left"], width-1))
    right = max(0, min(line_positions["right"], width-1))

    cv2.line(frame, (left, top), (left, bottom), (0, 255, 0), line_thickness)
    cv2.line(frame, (right, top), (right, bottom), (0, 255, 0), line_thickness)
    cv2.line(frame, (left, top), (right, top), (255, 0, 0), line_thickness)
    cv2.line(frame, (left, bottom), (right, bottom), (255, 0, 0), line_thickness)

    # Sayaçları ekranda göster
    cv2.putText(frame, f"Ust: {counts['top']}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
    cv2.putText(frame, f"Alt: {counts['bottom']}", (10, 60), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
    cv2.putText(frame, f"Sol: {counts['left']}", (10, 90), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
    cv2.putText(frame, f"Sag: {counts['right']}", (10, 120), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)

    for obj in tracked_objects:
        cv2.putText(frame, f"ID:{obj['id']}", (obj['center'][0] + 10, obj['center'][1] - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 2)

    cv2.imshow("Kamera ile Sayim", frame)

    # ---------- ARDUINO'YA GÖNDERME ----------
    try:
        msg = f"{counts['right']}:{counts['left']}:{counts['bottom']}:{counts['top']}"
        send_text(msg)

    except Exception as e:
        print("Arduino'ya veri gönderilemedi:", e)

    key = cv2.waitKey(10) & 0xFF
    if key == 27:
        break
    if key == ord('*'):
        counts = {"top":0, "bottom":0, "left":0, "right":0}
        print("Sayımlar sıfırlandı klavyeden!")

cap.release()
cv2.destroyAllWindows()
print("Program sonlandırıldı.")
