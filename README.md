# Quadraphonic Boids Mixer

A quadraphonic mixer for the [Daisy Patch](https://electro-smith.com/products/patch) Eurorack module. Two audio inputs are positioned in a four-channel sound field via CV — CV1/CV2 control the X/Y position of Audio In 1, CV3/CV4 control Audio In 2. Two additional inputs follow pre-defined internal paths (circular, figure-8, and others).

## Dependencies

This project requires **libDaisy** and **DaisySP** to live as sibling directories alongside `quad-boids-mixer/` — one level up, not inside this repo.

```
daisy_patch/              ← shared parent directory
├── libDaisy/             ← clone here
├── DaisySP/              ← clone here
└── quad-boids-mixer/     ← this repo
    └── src/
```

**One-time setup (run from the shared parent directory):**

```bash
git clone https://github.com/electro-smith/libDaisy.git
git clone https://github.com/electro-smith/DaisySP.git
cd libDaisy && git submodule update --init --recursive && make && cd ..
cd DaisySP && make && cd ..
```

You only need to do this once — all Daisy projects in the same parent directory share these builds.

## Build & flash

```bash
# From quad-boids-mixer/
make clean && make
make program
```

## Controls

| Control | Function |
|---------|----------|
| CV1     | Audio In 1 — X position (left/right) |
| CV2     | Audio In 1 — Y position (front/rear) |
| CV3     | Audio In 2 — X position |
| CV4     | Audio In 2 — Y position |
| Encoder | Select movement path for Inputs 3 & 4 (circular, figure-8, ...) |

## Audio I/O

| Jack    | Signal |
|---------|--------|
| In 1–2  | CV-positioned sources |
| In 3–4  | Path-automated sources |
| Out 1–4 | Front-Left, Front-Right, Rear-Left, Rear-Right |

## Hardware

[Electrosmith Daisy Patch](https://electro-smith.com/products/patch) — STM32H750 Cortex-M7 @ 480 MHz, 4 audio in/out, 4 CV inputs, encoder, 128×64 OLED.
