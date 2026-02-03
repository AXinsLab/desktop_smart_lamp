

# Sunlike Eye-Care Desk Lamp

[中文](README_CN.md) | English

Developed an eye-care desk lamp with dual interaction modes (wireless control and sliding control) using Seoul Semiconductor Sunlike LED beads. The structure is flexible and can be adjusted according to the actual usage environment. The controller is based on ESP32-C2 module and developed using the Arduino framework.
Copyright Notice: This project allows users to view, use, and modify the source code. Commercial use of the project or its derivatives (e.g., selling, profiting as part of commercial services) is prohibited. When using or distributing (including modified versions), the original copyright and license statement must be retained (attribution required).

<!-- PROJECT SHIELDS -->

[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]
[![BiliBili][bilibili-shield]][bilibili-url]

<!-- PROJECT LOGO -->
<br />

<p align="center">
  <a href="https://github.com/grant-Gan/desktop_smart_lamp/">
    <img src="Images/Logo.png" alt="Logo" width="80" height="80">
  </a>
  <h3 align="center">AXin Lab</h3>
  <p align="center">
    Never stop tinkering. AXin is dedicated to DIYing products we want but can't afford~
    <br />
    <a href="https://github.com/grant-Gan/desktop_smart_lamp"><strong>Explore the documentation »</strong></a>
    <br />
    <br />
    <a href="https://github.com/grant-Gan/desktop_smart_lamp">View Demo</a>
    ·
    <a href="https://github.com/grant-Gan/desktop_smart_lamp/issues">Report Bug</a>
    ·
    <a href="https://github.com/grant-Gan/desktop_smart_lamp/issues">Request Feature</a>
  </p>

</p>


## Table of Contents

- [Important Notes](#important-notes)
- [Download Steps](#download-steps)
- [File Directory Description](#file-directory-description)
- [Framework Used](#framework-used)
- [Dependencies](#dependencies)
- [Version Control](#version-control)
- [Component Selection](#component-selection)
    1. [Power Adapter](#power-adapter)
    2. [Data Cable](#data-cable)
    3. [Light Tube](#light-tube)
    4. [L-Bracket](#l-bracket)
    5. [Cantilever Bracket](#cantilever-bracket)
    6. [Others](#others)
- [PCB Soldering](#pcb-soldering)
    1. [Wireless Control Version](#wireless-control-version)
    2. [Sliding Control Version](#sliding-control-version)
- [Author](#author)
- [License](#license)

### Important Notes

**Features Under Development:**
1. ~~Controller Deep Sleep mode fix~~ Completed
2. ~~Automatic pairing between controller and driver board~~ Completed
3. Low battery reminder for controller
4. ~~HomeAssistant integration~~ Abandoned, no demand, don't want to work on it anymore

### Download Steps
1. Download ``Code\Lamp_Driver\Lamp_Driver.bin``
2. Download ``Code\rotary_controller\rotary_controller.bin``
3. Use the ``idf.py flash`` tool to directly flash to the corresponding board. Automatic pairing is implemented. For first-time use, manual pairing may be required once. See **Interaction Operations** for manual pairing instructions.

### Interaction Operations
- **Rotate encoder clockwise/counterclockwise:** Brighten/Dim the light
- **Press encoder button + Rotate encoder:** Adjust color temperature
- **Single click button/rotary encoder:** Turn on light
- **Long press encoder button for 2S:** Turn off light
- **Long press encoder button for 5S:** Re-pair



### File Directory Description

```
desktop_smart_lamp
├── README.md
├── LICENSE
├── /3D_Model
│  ├── /Lamp_case
│  ├── /Rotary_Controller
│  ├── /Slider_Controller
├── /BOM
│  ├── /BOM_Base_Board_2025-07-23.xlsx
│  ├── /BOM_Driver Board _2025-07-23.xlsx
│  ├── /BOM_Rotary_Controller_Board_2025-07-23.xlsx
│  ├── /BOM_Slider_Controller_Board_2025-07-23.xlsx
├── /images
├── /Code
│  ├── /Lamp_Driver
│  ├── /rotary_controller
├── /PCBA
│  ├── /Gerber_Base_Board_20250723.zip
│  ├── /Gerber_Driver_Board_20250723.zip
│  ├── /Gerber_Rotary_Controller_Board_20250723.zip
│  ├── /Gerber_Slider_Controller_Board_20250723.zip
├── /Schematic
│  ├── /Base_board.pdf
│  ├── /Driver_board.pdf
│  ├── /Rotary_Controller_Board.pdf
│  ├── /Slider_Controller_board.pdf

```

### Framework Used
[esp-idf](https://github.com/espressif/esp-idf)


### Dependencies

- [arduino-esp32](https://github.com/espressif/arduino-esp32)
- [Button2](https://github.com/LennartHennigs/Button2)
- [ai-esp32-rotary-encoder](https://github.com/igorantolic/ai-esp32-rotary-encoder)


### Version Control

This project uses Git for version management. You can view the currently available versions in the repository.

### Component Selection
#### Power Adapter
The maximum power consumption of this project is 30W, with an input voltage of 12~20V. At 20V, the efficiency is higher and the driver board generates less heat, which is recommended as the first choice. Therefore, a power adapter rated at 30W or above, supporting 20V output and PD charging protocol is required. Most fast charging adapters that come with mobile phones, if rated above 30W and have a TYPE-C interface, can be used without additional purchase.
<p align="center">
  <img src="Images/电源适配器.jpg" height="200">
  <p align="center">Power adapter with Type-C output, usually supports PD protocol</p>
</p>

#### Data Cable
This project uses a C2C fast charging data cable. It is recommended to use one rated at 60W or above. The length should be selected according to the usage environment. AXin uses a 2-meter data cable, which can match most usage scenarios.
<p align="center">
  <img src="Images/快充数据线.webp" height="200">
  <p align="center">C2C Fast Charging Data Cable</p>
</p>

#### Light Tube
This project uses a 70cm long aluminum alloy light tube with a cross-section width of 26mm and height of 11mm. Purchase link can be found in the Bilibili video. The merchant's default selling length is 1 meter, please leave a message for cutting requirements.

#### L-Bracket
This project uses a 20*32*60 L-shaped stainless steel bracket. If you want to use other models, ensure that the screw hole opening is larger than 6.0mm so that the 1/4 screw head on the cantilever bracket can fit through.
<p align="center">
  <img src="Images/角码.png" height="300">
  <p align="center">20x32x60 L-Bracket</p>
</p>

#### Cantilever Bracket
This project uses a cantilever bracket with a 1/4 screw paired with an aluminum clamp. Note that due to the long light tube, the moment arm is large. You must choose a cantilever bracket of sufficient quality, otherwise the nut fixing the wrench will strip. It is recommended not to be cheap and purchase from regular channels.
<p align="center">
  <img src="Images/悬臂支架.png" height="300">
  <p align="center">Cantilever bracket with 1/4 screw</p>
</p>

#### Screws
- Wireless Control Version

| Location      | Model      | Type   | Screw Head Type | Quantity |\n|---------|---------|------|-------|----|\n| End cap to light tube connection | M2.6*20 | Self-tapping screw | Hex socket   | 4  |\n| End cap top-bottom connection  | M2*12   | Regular screw | Hex socket   | 2  |\n| End cap top-bottom connection  | M2      | Nut   | \     | 2  |\n| Controller     | M2*12   | Self-tapping screw | Hex socket   | 5  |\n| Light tube to L-bracket connection | M6*6    | Regular screw | Hex socket   | 2  |\n| Light tube to L-bracket connection | M6*10*2 | Square nut | \     | 2  |\n| L-bracket to bracket connection | 1/4-20 thread | Anti-slip nut | \     | 1  |\n| L-bracket to bracket connection | M6*10*2 | Washer   | \     | 1  |

- Sliding Control Version

| Location       | Model      | Type   | Screw Head Type | Quantity |
|----------|---------|------|-------|----|\n| End cap to light tube connection  | M2.6*20 | Self-tapping screw | Hex socket   | 4  |\n| End cap to controller connection | M2*25   | Regular screw | Hex socket   | 2  |\n| End cap top-bottom connection   | M2      | Nut   | \     | 2  |\n| Controller to light tube connection | M2*18   | Self-tapping screw | Hex socket   | 2  |\n| Light tube to L-bracket connection | M6*6    | Regular screw | Hex socket   | 2  |\n| Light tube to L-bracket connection | M6*10*2 | Square nut | \     | 2  |\n| L-bracket to bracket connection | 1/4-20 thread | Anti-slip nut | \     | 1  |\n| L-bracket to bracket connection | M6*10*2 | Washer   | \     | 1  |


#### Others
There are currently no other components that require special attention. Will add more if remembered or if many friends ask about it later~

### PCB Soldering

#### Wireless Control Version
- Driver Board
The wireless control driver board uses the ESP32-C2 module, PWM dimming, and the driver chip is LGS63042EP. In the middle section, only R9, R18 pull-down resistors and D5, D6 zener diodes need to be soldered. The zener diodes are used to protect the driver chip EN pin and are optional.
**Note: The driver output voltage is relatively high, capacitors rated at 50V or above must be used.**
<p align="center">
  <img src="Images/PWM调光版-焊接参考.png">
Wireless control version driver board soldering reference
</p>

- Control Board
**Note: When soldering the controller, pay attention to the encoder model selection. The encoder model used in this project is EC11, handle length 12mm, plum handle, positive code (clockwise), 20 pulses per rotation**

<p align="center">
  <img src="Images/控制板TOP.png" width="300" hwight="300">
  <p align="center">Controller soldering reference</p>
</p>

<p align="center">
  <img src="Images/控制板Buttom.png" width="300" hwight="300">
  <p align="center">Controller soldering reference</p>
</p>


#### Sliding Control Version
- Driver Board
For the sliding control version, you can skip soldering the controller and DCDC components, which reduces the cost considerably. If you don't need wireless control, this is recommended as the first choice. For specific components to solder, please refer to the soldering reference diagram and schematic below.
**Note: The driver output voltage is relatively high, capacitors rated at 50V or above must be used.**
<p align="center">
  <img src="Images/模拟调光版-焊接参考.png">
Sliding control version driver board soldering reference
</p>

- Sliding Control Board
Nothing special here, just solder according to the diagram.
<p align="center">
  <img src="Images/滑动控制板.png" width="300">
  <p align="center">Sliding control board</p>
</p>



### Author

AXin Lab

Bilibili: @AXin实验室 XiaoHongShu: @AXin实验室  Xianyu: @AXin实验室

### License
This project is licensed under the **Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License**.
You are free to:
*   **Share** — copy and redistribute the material in any medium or format
*   **Adapt** — remix, transform, and build upon the material

Under the following terms:
*   **Attribution** — You must give [appropriate credit](https://creativecommons.org/licenses/by-nc-sa/4.0/deed.en), provide a link to the license, and indicate if changes were made. You may do so in any reasonable manner, but not in any way that suggests the licensor endorses you or your use.
*   **NonCommercial** — You may not use the material for commercial purposes.
*   **ShareAlike** — If you remix, transform, or build upon the material, you must distribute your contributions under the [same license](https://creativecommons.org/licenses/by-nc-sa/4.0/deed.en) as the original.

For the complete license terms, please see: [LICENSE](LICENSE) file.

This project is signed under MIT License, see [LICENSE.txt](https://github.com/grant-Gan/desktop_smart_lamp/LICENSE.txt) for details



<!-- links -->
[your-project-path]:grant-Gan/desktop_smart_lamp
[contributors-shield]: https://img.shields.io/github/contributors/grant-Gan/desktop_smart_lamp.svg?style=flat-square
[contributors-url]: https://github.com/grant-Gan/desktop_smart_lamp/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/grant-Gan/desktop_smart_lamp.svg?style=flat-square
[forks-url]: https://github.com/grant-Gan/desktop_smart_lamp/network/members
[stars-shield]: https://img.shields.io/github/stars/grant-Gan/desktop_smart_lamp.svg?style=flat-square
[stars-url]: https://github.com/grant-Gan/desktop_smart_lamp/stargazers
[issues-shield]: https://img.shields.io/github/issues/grant-Gan/desktop_smart_lamp.svg?style=flat-square
[issues-url]: https://github.com/grant-Gan/desktop_smart_lamp/issues
[license-shield]: https://img.shields.io/github/license/grant-Gan/desktop_smart_lamp.svg?style=flat-square
[license-url]: https://github.com/grant-Gan/desktop_smart_lamp/blob/master/LICENSE.txt
[bilibili-shield]: https://img.shields.io/badge/关注我-FB7299?logo=bilibili&logoColor=white
[bilibili-url]: https://www.bilibili.com/video/BV1rygnzuE4w



