# GreenGuard Model 1

## Edge Impulse Project
- Project name: GreenGuard-Model-1
- Classes: healthy | early_sick | critical
- Dataset: PlantVillage (Tomato subset)
- Training images: 120 per class (360 total)
- Test images: 30 per class (90 total)

## Architecture
- Input: 96x96 RGB
- Model: MobileNetV1 96x96 0.25
- Output: 3 classes

## Performance
- Training accuracy: 77.8%
- Test accuracy:     86.67%
- ROC AUC:          0.96
- F1 Score:         0.89

## Status
- [x] Dataset uploaded
- [x] Impulse designed
- [x] Model trained
- [x] Model tested
- [x] Arduino library exported
- [ ] Flashed to senseBox Eye 2
