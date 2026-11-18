# Pocket Companion

Raise Ember, a pixel companion, on the round touch screen.

## How to play

- **Tap Ember**: pet it
- **Feed**: feed it; it refuses when it is full
- **Play**: play together; uses energy and lifts its mood
- **Sleep / Wake up**: switch to the night scene to sleep; Ember wakes up on its own once rested
- **Stats**: view the five stats (hunger, mood, energy, cleanliness, health) and the current level
- **Clean**: clean up droppings

## Growth

Ember grows only while you keep it company in the foreground; each level takes 30 minutes. Levels 1–10 are Baby, 11–30 Growing, and 31–50 Adult. Growth pauses while you are away from the app or the control center is open, so Ember never starves while the device is off. Neglect lowers health to a recoverable floor of 20; this version has no death.

## Saving

Progress is stored in a dedicated NVS save with two checksummed records. It saves automatically after actions, and also before returning to the launcher and before power-off. A sudden power loss may lose progress made since the last successful save.

Fully offline with an English UI. The care logic is based on the MIT-licensed epet; Ember is an original pixel character.
