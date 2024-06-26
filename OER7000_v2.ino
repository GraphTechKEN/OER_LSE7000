//Adafruit MCP23017 Arduino Library を導入してください
//Arduino Micro または Leonard を使用してください

//簡単な説明
//マスコン5段、ブレーキ8段+EBです。それ以外は使用不可です。(今のところ)
//ノッチの移動量に応じて、各キーボードコマンドを打ち込んでいます。
//MC53側は真理値に基づいてノッチ(N,P1～P5)を指示します。レバーサも対応します。
//ME38側はポテンショの値を一旦角度換算し、ブレーキノッチ(N,B1～EB)を指示します。

//BVEゲーム開始時は、一旦ブレーキハンドルをN→B8、マスコンノッチはN、レバーさハンドルをB→N→Fと動かす等してリセットします。

#include <Adafruit_MCP23X17.h>
#include <NintendoSwitchControlLibrary.h>
#include <Keyboard.h>
//KeyboardライブラリがNintendoSwitchcControlLibraryでキーコードがSwitch用に上書きされていることに注意

// https://github.com/lefmarna/NintendoSwitchControlLibrary
//VID 0F0D
//PID 00C1
//C:\Program Files (x86)\Arduino\hardware\arduino\avr\boards.txt 内、
// micro.vid.1=0x0F0D
// micro.pid.1=0x00C1
// micro.build.vid=0x0F0D
// micro.build.pid=0x00C1
//へ変更

//ボタンピンアサイン
#define btn_A 0
#define btn_B 2
#define btn_X 6
#define btn_Y 7

#define btn_RR 15
#define btn_Home 9
#define btn_ZR 13
#define btn_Plus 12

#define btn_R 3
#define btn_D 1
#define btn_U 5
#define btn_L 4

#define btn_Minus 10
#define btn_Capture 8
#define btn_ZL 14
#define btn_LL 11

#define PIN_DIR_F A0
#define PIN_DIR_B A1

#define PIN_MC_NOTCH_1 A3
#define PIN_MC_NOTCH_2 A4
#define PIN_MC_NOTCH_3 A5
#define PIN_MC_NOTCH_4 12

#define PIN_BRK_NOTCH_1 2
#define PIN_BRK_NOTCH_3 4
#define PIN_BRK_NOTCH_5 6
#define PIN_BRK_NOTCH_7 8
#define PIN_BRK_NOTCH_8 9
#define PIN_EB 10

#define PIN_BVE_MODE 11

#define PIN_YOKUSOKU_MODE 7

Adafruit_MCP23X17 mcp_DG;
SPISettings settings = SPISettings(1000000, MSBFIRST, SPI_MODE0);

uint16_t ioexp_2_AB = 0;

bool modeBVE = false;

//ボタン設定
bool btnSelect = false;
bool btnStart = false;
bool btnA = false;
bool btnB = false;
bool btnC = false;
bool btnD = false;
bool btnSelect_latch = false;
bool btnStart_latch = false;
bool btnA_latch = false;
bool btnB_latch = false;
bool btnC_latch = false;
bool btnD_latch = false;

bool btn_hat_right = false;
bool btn_hat_up = false;
bool btn_hat_down = false;
bool btn_hat_left = false;

bool yokusoku = false;

//レバーサ設定
int8_t iDir_latch = false;

uint8_t mc_notch = 0;
uint8_t mc_notch_latch = 0;

uint8_t brk_notch = 0;
uint8_t brk_notch_latch = 0;

uint16_t ioexp_2_AB_latch = false;

void setup() {
  //Serial.begin(115200);
  //Serial.setTimeout(10);
  Keyboard.begin();
  pinMode(SS, OUTPUT);

  pinMode(PIN_DIR_F, INPUT_PULLUP);
  pinMode(PIN_DIR_B, INPUT_PULLUP);

  pinMode(PIN_MC_NOTCH_1, INPUT_PULLUP);
  pinMode(PIN_MC_NOTCH_2, INPUT_PULLUP);
  pinMode(PIN_MC_NOTCH_3, INPUT_PULLUP);
  pinMode(PIN_MC_NOTCH_4, INPUT_PULLUP);

  pinMode(PIN_BRK_NOTCH_1, INPUT_PULLUP);
  pinMode(PIN_BRK_NOTCH_3, INPUT_PULLUP);
  pinMode(PIN_BRK_NOTCH_5, INPUT_PULLUP);

  pinMode(PIN_BRK_NOTCH_7, INPUT_PULLUP);
  pinMode(PIN_BRK_NOTCH_8, INPUT_PULLUP);
  pinMode(PIN_EB, INPUT_PULLUP);

  pinMode(PIN_BVE_MODE, INPUT_PULLUP);

  pinMode(PIN_YOKUSOKU_MODE, INPUT_PULLUP);

  if (!mcp_DG.begin_SPI(SS)) {
    //Serial.println("Error.");
    while (1)
      ;
  }

  // DenGo_Keyを全てプルアップ
  for (int i = 0; i < 16; i++) {
    mcp_DG.pinMode(i, INPUT_PULLUP);
  }
}

void loop() {
  // LOW = pressed, HIGH = not pressed
  //シリアルモニタが止まるのを防止するおまじない
  /*if (Serial.available()) {
    Serial.read();
  }*/
  modeBVE = read_Mode();

  read_DIR();
  brk_notch = read_Break();
  mc_notch = read_MC();
  if (!read_EB() && mc_notch > 0) {
    brk_notch = 9;
    mc_notch = 0;
    if (modeBVE) {
      set_Break_Bve(brk_notch);
    } else {
      set_Break_Switch(brk_notch);
    }
  } else {

    if (mc_notch == 0) {
      if (modeBVE) {
        set_Break_Bve(brk_notch);
      } else {
        set_Break_Switch(brk_notch);
      }
    }
    if (modeBVE) {
      set_MC_Bve(mc_notch);
    } else {
      set_MC_Switch(mc_notch);
    }
  }

  mc_notch_latch = mc_notch;
  brk_notch_latch = brk_notch;

  read_IOexp();  //IOエキスパンダ読込ルーチン
  Buttons();     //ボタン読込ルーチン

  delay(10);
}

bool read_EB() {
  return !digitalRead(PIN_EB);
}

bool read_Mode() {
  return !digitalRead(PIN_BVE_MODE);
}

uint8_t read_Break() {
  bool PIN_BRK_1 = !digitalRead(PIN_BRK_NOTCH_1);
  bool PIN_BRK_3 = !digitalRead(PIN_BRK_NOTCH_3);
  bool PIN_BRK_5 = !digitalRead(PIN_BRK_NOTCH_5);
  bool PIN_BRK_7 = !digitalRead(PIN_BRK_NOTCH_7);
  bool PIN_BRK_8 = !digitalRead(PIN_BRK_NOTCH_8);

  bool Yokusoku_Mode = !digitalRead(PIN_YOKUSOKU_MODE);

  uint8_t _brk_notch = 0;

  if (mc_notch == 0 && PIN_BRK_1 && PIN_BRK_3 && !PIN_BRK_5 && PIN_BRK_7 && PIN_BRK_8) {
    if (modeBVE || Yokusoku_Mode) {
      _brk_notch = 1;
    } else {
      _brk_notch = 2;
      yokusoku = 0;
    }
  } else if (mc_notch == 0 && PIN_BRK_1 && !PIN_BRK_3 && PIN_BRK_5 && PIN_BRK_7 && PIN_BRK_8) {
    if (modeBVE || Yokusoku_Mode) {
      _brk_notch = 2;
    } else {
      _brk_notch = 3;
      yokusoku = 0;
    }
  } else if (mc_notch == 0 && PIN_BRK_1 && PIN_BRK_3 && PIN_BRK_5 && PIN_BRK_7 && PIN_BRK_8) {
    if (modeBVE || Yokusoku_Mode) {
      _brk_notch = 3;
    } else {
      _brk_notch = 4;
      yokusoku = 0;
    }
  } else if (mc_notch == 0 && !PIN_BRK_1 && !PIN_BRK_3 && !PIN_BRK_5 && PIN_BRK_7 && PIN_BRK_8) {
    if (modeBVE || Yokusoku_Mode) {
      _brk_notch = 4;
    } else {
      _brk_notch = 5;
      yokusoku = 0;
    }
  } else if (mc_notch == 0 && !PIN_BRK_1 && PIN_BRK_3 && !PIN_BRK_5 && PIN_BRK_7 && PIN_BRK_8) {
    if (modeBVE || Yokusoku_Mode) {
      _brk_notch = 5;
    } else {
      _brk_notch = 6;
      yokusoku = 0;
    }
  } else if (mc_notch == 0 && !PIN_BRK_1 && !PIN_BRK_3 && PIN_BRK_5 && PIN_BRK_7 && PIN_BRK_8) {
    if (modeBVE || Yokusoku_Mode) {
      _brk_notch = 6;
    } else {
      _brk_notch = 7;
      yokusoku = 0;
    }
  } else if (mc_notch == 0 && !PIN_BRK_1 && PIN_BRK_3 && PIN_BRK_5 && PIN_BRK_7 && PIN_BRK_8) {
    if (modeBVE || Yokusoku_Mode) {
      _brk_notch = 7;
    } else {
      _brk_notch = 8;
      yokusoku = 0;
    }

  } else if (mc_notch == 0 && !PIN_BRK_1 && !PIN_BRK_3 && !PIN_BRK_5 && !PIN_BRK_7 && PIN_BRK_8) {
    _brk_notch = 9;
    if (modeBVE || Yokusoku_Mode) {
    } else {
      yokusoku = 0;
    }

  } else {
    if (!modeBVE) {
      if (yokusoku) {
        _brk_notch = 1;
      } else {
        _brk_notch = 0;
      }
    }
  }

  /*
  Serial.print(PIN_BRK_1);
  Serial.print(PIN_BRK_2);
  Serial.print(PIN_BRK_3);
  Serial.print(PIN_BRK_4);
  Serial.print(PIN_BRK_5);
  Serial.print(PIN_BRK_6);
  Serial.print(PIN_BRK_7);
  Serial.print(PIN_BRK_8);
  */
  //Serial.print(" BRK_notch:");
  //Serial.println(_brk_notch);
  //Serial.print(" MC_notch:");
  //Serial.println(mc_notch);

  return _brk_notch;
}

void set_Break_Switch(uint8_t _brk_notch) {

  if (_brk_notch != brk_notch_latch) {
    //Serial.print(" BRK_notch(Switch):");
    //Serial.println(_brk_notch);

    switch (_brk_notch) {
      case 9:
        SwitchControlLibrary().moveLeftStick(0x80, 0x00);
        SwitchControlLibrary().moveRightStick(0xFF, 0x80);
        SwitchControlLibrary().sendReport();
        break;
      case 8:
        SwitchControlLibrary().moveLeftStick(0x80, 0x05);
        SwitchControlLibrary().moveRightStick(0xFB, 0x80);
        SwitchControlLibrary().sendReport();
        break;
      case 7:
        SwitchControlLibrary().moveLeftStick(0x80, 0x13);
        SwitchControlLibrary().moveRightStick(0xED, 0x80);
        SwitchControlLibrary().sendReport();
        break;
      case 6:
        SwitchControlLibrary().moveLeftStick(0x80, 0x20);
        SwitchControlLibrary().moveRightStick(0xE0, 0x80);
        SwitchControlLibrary().sendReport();
        break;
      case 5:
        SwitchControlLibrary().moveLeftStick(0x80, 0x2E);
        SwitchControlLibrary().moveRightStick(0xD2, 0x80);
        SwitchControlLibrary().sendReport();
        break;
      case 4:
        SwitchControlLibrary().moveLeftStick(0x80, 0x3C);
        SwitchControlLibrary().moveRightStick(0xC4, 0x80);
        SwitchControlLibrary().sendReport();
        break;
      case 3:
        SwitchControlLibrary().moveLeftStick(0x80, 0x49);
        SwitchControlLibrary().moveRightStick(0xB7, 0x80);
        SwitchControlLibrary().sendReport();
        break;
      case 2:
        SwitchControlLibrary().moveLeftStick(0x80, 0x57);
        SwitchControlLibrary().moveRightStick(0xA9, 0x80);
        SwitchControlLibrary().sendReport();
        break;
      case 1:
        SwitchControlLibrary().moveLeftStick(0x80, 0x65);
        SwitchControlLibrary().moveRightStick(0x9D, 0x80);
        SwitchControlLibrary().sendReport();
        break;
      case 0:
        SwitchControlLibrary().moveLeftStick(0x80, 0x80);
        SwitchControlLibrary().moveRightStick(0x80, 0x80);
        SwitchControlLibrary().sendReport();
        break;
    }
  }
}

void set_Break_Bve(uint8_t _brk_notch) {

  if (_brk_notch != brk_notch_latch) {
    Serial.print(" BRK_notch(BVE):");
    Serial.println(_brk_notch);
    if (_brk_notch == 9) {
      Keyboard.write('/');
    } else {
      int8_t count = abs(_brk_notch - brk_notch_latch);
      for (int i = 0; i < count; i++) {
        if ((_brk_notch - brk_notch_latch) > 0) {
          Keyboard.write('.');
        } else {
          Keyboard.write(',');
        }
      }
    }
  }
}

void set_MC_Bve(uint8_t _mc_notch) {
  if (_mc_notch != mc_notch_latch) {
    //Serial.print(" MC_notch:");
    //Serial.println(_mc_notch);
    int8_t count = abs(_mc_notch - mc_notch_latch);
    for (int i = 0; i < count; i++) {
      if ((_mc_notch - mc_notch_latch) > 0) {
        Keyboard.write('Z');
      } else {
        Keyboard.write('A');
      }
    }
  }
}

uint8_t read_MC() {
  bool MC_Notch_1 = !digitalRead(PIN_MC_NOTCH_1);
  bool MC_Notch_2 = !digitalRead(PIN_MC_NOTCH_2);
  bool MC_Notch_3 = !digitalRead(PIN_MC_NOTCH_3);
  bool MC_Notch_4 = !digitalRead(PIN_MC_NOTCH_4);
  uint8_t _mc_notch = 0;
  if (MC_Notch_4) {
    _mc_notch = 4;
    yokusoku = 0;
  } else if (MC_Notch_3) {
    _mc_notch = 3;
    yokusoku = 0;
  } else if (MC_Notch_2) {
    _mc_notch = 2;
    yokusoku = 0;
  } else if (MC_Notch_1) {
    _mc_notch = 1;
    yokusoku = 0;
  }
  /*
  Serial.print(MC_Notch_1);
  Serial.print(MC_Notch_2);
  Serial.print(MC_Notch_3);
  Serial.print(MC_Notch_4);
  Serial.print(" MC_Notch:");
  Serial.println(_mc_notch);
*/
  return _mc_notch;
}

void set_MC_Switch(uint8_t _mc_notch) {
  if (_mc_notch != mc_notch_latch) {
    Serial.print("MC_notch:");
    Serial.println(_mc_notch);
    //          mode103 = digitalRead(13);
    bool mode103 = false;
    switch (_mc_notch) {
      case 0:
        SwitchControlLibrary().moveLeftStick(0x80, 0x80);
        SwitchControlLibrary().sendReport();
        break;
      case 1:
        if (!mode103) {
          SwitchControlLibrary().moveLeftStick(0x80, 0x9F);
        } else {
          SwitchControlLibrary().moveLeftStick(0x60, 0x80);
        }
        SwitchControlLibrary().sendReport();
        break;
      case 2:
        if (!mode103) {

          SwitchControlLibrary().moveLeftStick(0x80, 0xB7);
        } else {
          SwitchControlLibrary().moveLeftStick(0x40, 0x80);
        }
        SwitchControlLibrary().sendReport();
        break;
      case 3:
        if (!mode103) {
          SwitchControlLibrary().moveLeftStick(0x80, 0xCE);
        } else {
          SwitchControlLibrary().moveLeftStick(0x20, 0x80);
        }
        SwitchControlLibrary().sendReport();
        break;
      case 4:
        if (!mode103) {
          SwitchControlLibrary().moveLeftStick(0x80, 0xE6);
        } else {
          SwitchControlLibrary().moveLeftStick(0x00, 0x80);
        }
        SwitchControlLibrary().sendReport();
        break;
      case 5:
        if (!mode103) {
          SwitchControlLibrary().moveLeftStick(0x80, 0xFF);
        } else {
          SwitchControlLibrary().moveLeftStick(0x00, 0x80);
        }
        SwitchControlLibrary().sendReport();
        break;
    }
  }
}

void read_DIR() {
  bool Dir_F = !digitalRead(PIN_DIR_F);
  bool Dir_B = !digitalRead(PIN_DIR_B);

  int8_t iDir = 0;
  //前進に切り替えた時
  if (Dir_F) {
    //Serial.println("Dir F");
    iDir = 1;
  }
  //後進に切り替えた時
  if (Dir_B) {
    //Serial.println("Dir B");
    iDir = -1;
  }

  //レバーサが前回と異なるとき
  if (iDir != iDir_latch) {
    if (modeBVE) {
      uint8_t d = abs(iDir - iDir_latch);

      //Serial.print(" iDir: ");
      //Serial.print(iDir);

      for (uint8_t i = 0; i < d; i++) {
        //前進
        if ((iDir - iDir_latch) > 0) {
          Keyboard.write(0xDA);  //"↑"
          //Serial.println("↑");
          //後退
        } else {
          Keyboard.write(0xD9);  //"↓"
          //Serial.println("↓");
        }
      }
    }
  }
  iDir_latch = iDir;
}



//MCP23S17読込
//値が0でないとき　かつ n 回連続で同じ値を出力したときに値を変更する
void read_IOexp() {
  uint16_t temp_ioexp2_ini = mcp_DG.readGPIOAB();
  int n = 3;
  if (temp_ioexp2_ini != 0) {
    for (int i = 0; i < n; i++) {
      uint16_t temp_ioexp2 = mcp_DG.readGPIOAB();
      if (temp_ioexp2 != 0) {
        if (temp_ioexp2_ini == temp_ioexp2) {
          if (i == n - 1) {
            ioexp_2_AB = temp_ioexp2;
          }
        } else {
          break;
        }
      } else {
        break;
      }
    }
  }
}

void Buttons(void) {
  if (ioexp_2_AB != ioexp_2_AB_latch) {
    //Aボタン
    if (~ioexp_2_AB & (1 << btn_A)) {
      if (modeBVE) {
        Keyboard.press(0xB0);  //Enter
      } else {
        SwitchControlLibrary().pressButton(Button::A);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (modeBVE) {
        Keyboard.release(0xB0);  //Enter
      } else {
        SwitchControlLibrary().releaseButton(Button::A);
        SwitchControlLibrary().sendReport();
      }
    }

    //Bボタン
    if (~ioexp_2_AB & (1 << btn_B)) {
      if (modeBVE) {
        Keyboard.press(KEY_KP_PLUS);  //"KP_PLUS" MH
      } else {
        SwitchControlLibrary().pressButton(Button::B);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (modeBVE) {
        Keyboard.release(KEY_KP_PLUS);  //"KP_PLUS" MH
      } else {
        SwitchControlLibrary().releaseButton(Button::B);
        SwitchControlLibrary().sendReport();
      }
    }

    //Xボタン
    if (~ioexp_2_AB & (1 << btn_X)) {
      if (modeBVE) {
        Keyboard.write(0xD2);  //"Home"
      } else {
        SwitchControlLibrary().pressButton(Button::X);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (!modeBVE) {
        SwitchControlLibrary().releaseButton(Button::X);
        SwitchControlLibrary().sendReport();
      }
    }

    //Yボタン
    if (~ioexp_2_AB & (1 << btn_Y)) {
      if (modeBVE) {
        Keyboard.press(0x20);  //"Space"
      } else {
        SwitchControlLibrary().pressButton(Button::Y);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (modeBVE) {
        Keyboard.release(0x20);  //"Space"
      } else {
        SwitchControlLibrary().releaseButton(Button::Y);
        SwitchControlLibrary().sendReport();
      }
    }

    //→ボタン
    if (~ioexp_2_AB & (1 << btn_R)) {
      btn_hat_right = true;
      if (modeBVE) {
        Keyboard.write(KEY_RIGHT_ARROW);
      } else {
        //pushHat(Hat::RIGHT);
        //SwitchControlLibrary().pressHatButton(Hat::RIGHT);
        //SwitchControlLibrary().sendReport();
      }
    } else {
      btn_hat_right = false;
      if (!modeBVE && (!btn_hat_left || !btn_hat_up || !btn_hat_down)) {
        //SwitchControlLibrary().releaseHatButton();
        //SwitchControlLibrary().sendReport();
      }
    }

    //↓ボタン
    if (~ioexp_2_AB & (1 << btn_D)) {
      btn_hat_down = true;
      if (modeBVE) {
        Keyboard.write(0xD9);  //↓
      } else {
        //pushHat(Hat::DOWN);
        //SwitchControlLibrary().pressHatButton(Hat::DOWN);
        //SwitchControlLibrary().sendReport();
      }
    } else {
      btn_hat_down = false;
      ;
      if (!modeBVE && (!btn_hat_left || !btn_hat_up || !btn_hat_right)) {
        //SwitchControlLibrary().releaseHatButton();
        //SwitchControlLibrary().sendReport();
      }
    }

    //↑ボタン
    if (~ioexp_2_AB & (1 << btn_U)) {
      btn_hat_up = true;
      if (modeBVE) {
        Keyboard.write(0xDA);  //↑
      } else {
        //pushHat(Hat::UP);
      }
    } else {
      btn_hat_up = false;
    }

    //←ボタン
    if (~ioexp_2_AB & (1 << btn_L)) {
      btn_hat_left = true;
      if (modeBVE) {
        Keyboard.write(KEY_LEFT_ARROW);
      } else {
        pushHat(Hat::LEFT);
      }
    } else {
      btn_hat_left = false;
    }

    //Rボタン
    if (~ioexp_2_AB & (1 << btn_RR)) {
      if (modeBVE) {
        Keyboard.write('/');
      } else {
        SwitchControlLibrary().pressButton(Button::R);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (!modeBVE) {
        SwitchControlLibrary().releaseButton(Button::R);
        SwitchControlLibrary().sendReport();
      }
    }

    //Homeボタン
    if (~ioexp_2_AB & (1 << btn_Home)) {
      if (modeBVE) {
        Keyboard.write(0xD1);  //"Insert"
      } else {
        SwitchControlLibrary().pressButton(Button::HOME);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (!modeBVE) {
        SwitchControlLibrary().releaseButton(Button::HOME);
        SwitchControlLibrary().sendReport();
      }
    }

    //ZRボタン
    if (~ioexp_2_AB & (1 << btn_ZR)) {
      if (modeBVE) {
        Keyboard.write('.');
      } else {
        SwitchControlLibrary().pressButton(Button::ZR);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (!modeBVE) {
        SwitchControlLibrary().releaseButton(Button::ZR);
        SwitchControlLibrary().sendReport();
      }
    }

    //+ボタン
    if (~ioexp_2_AB & (1 << btn_Plus)) {
      if (modeBVE) {
        Keyboard.write(',');
      } else {
        SwitchControlLibrary().pressButton(Button::PLUS);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (!modeBVE) {
        SwitchControlLibrary().releaseButton(Button::PLUS);
        SwitchControlLibrary().sendReport();
      }
    }

    //-ボタン
    if (~ioexp_2_AB & (1 << btn_Minus)) {
      if (modeBVE) {
        //Keyboard.write(',');
      } else {
        pushButton(Button::MINUS);
        if (mc_notch == 0) {
          yokusoku = 1;
        }
      }
    }


    //Captureボタン
    if (~ioexp_2_AB & (1 << btn_Capture)) {
      if (modeBVE) {
        Keyboard.press(KEY_DELETE);  //"Delete" EB
      } else {
        SwitchControlLibrary().pressButton(Button::CAPTURE);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (modeBVE) {
        Keyboard.release(KEY_DELETE);  //"Delete"
      } else {
        SwitchControlLibrary().releaseButton(Button::CAPTURE);
        SwitchControlLibrary().sendReport();
      }
    }

    //ZLボタン
    if (~ioexp_2_AB & (1 << btn_ZL)) {
      if (modeBVE) {
        Keyboard.write('A');
      } else {
        SwitchControlLibrary().pressButton(Button::ZL);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (!modeBVE) {
        SwitchControlLibrary().releaseButton(Button::ZL);
        SwitchControlLibrary().sendReport();
      }
    }

    //LLボタン
    if (~ioexp_2_AB & (1 << btn_LL)) {
      if (modeBVE) {
        Keyboard.write('Z');
      } else {
        SwitchControlLibrary().pressButton(Button::L);
        SwitchControlLibrary().sendReport();
      }
    } else {
      if (!modeBVE) {
        SwitchControlLibrary().releaseButton(Button::L);
        SwitchControlLibrary().sendReport();
      }
    }

    if (!modeBVE) {
      if (btn_hat_right) {
        SwitchControlLibrary().pressHatButton(Hat::RIGHT);
      }
      if (btn_hat_down) {
        SwitchControlLibrary().pressHatButton(Hat::DOWN);
      }
      if (btn_hat_up) {
        SwitchControlLibrary().pressHatButton(Hat::UP);
      }
      if (btn_hat_left) {
        SwitchControlLibrary().pressHatButton(Hat::LEFT);
      }
      if (!btn_hat_right && !btn_hat_down && !btn_hat_left && !btn_hat_up) {
        SwitchControlLibrary().releaseHatButton();
      }
      SwitchControlLibrary().sendReport();
    }

    ioexp_2_AB_latch = ioexp_2_AB;
  }
}
