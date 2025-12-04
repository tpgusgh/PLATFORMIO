#include <Arduino.h>
#include <string.h>

//부호가 없는 16bit 정수
uint16_t num1 = 1234;

//부호가 있는 16bit 정수
int16_t num2 = -1234;

byte array1[2];

byte array2[] = {0xd2,0x04, 0x2e, 0xfb};


byte array3[] = {0x01, 0x04, 0x02, 0x01, 0x31, 0x79, 0x74};

byte array4[] = {0x01, 0x04, 0x04, 0x01, 0x31, 0x02, 0x22, 0x2A, 0xCE};
//공용체
union{
  uint16_t input;
  byte output[2];
}myunion;

struct{
  uint16_t num1;
  int16_t num2;
}mystruct;

union{
  byte array[4];
  struct
  {
    uint16_t num1;
    int16_t num2;
  };
  
}myunion2;

struct{
  uint8_t addr;
  uint8_t fc;
  uint8_t num;
  uint8_t data_l;
  uint8_t data_h;
  uint16_t crc;
}RES;


struct{
  uint8_t addr;
  uint8_t fc;
  uint8_t num;
  uint8_t tem_l;
  uint8_t tem_h;
  uint8_t humi_h;
  uint8_t humi_l;
  uint16_t crc;
}RES2;

struct{
  uint8_t addr = 0x01;
  uint8_t fc = 0x04;
  uint16_t reg_addr = 0x0100;
  uint16_t reg_cnt = 0x0100;
  uint16_t reg_crc = 0x0A60;
}MODbUS;



void setup() {
  Serial.begin(9600);
  //사칙연산(bit연산)
  array1[0] = num1/256; // num1의 상위 8비트
  array1[1] = num1%256; // num1의 하위 8비트


  // myunion.input = num1;
  myunion.output[0] = 0xD2;
  myunion.output[1] = 0x04;

  mystruct.num1 = num1;
  mystruct.num2 = num2;

  
  //array2의 4byte를 mystruct 메모리에 복사한다
  // memcpy(&mystruct, array2, sizeof(mystruct));
  //==
  //데이터 집어넣기?? (array2 -> myunion2)
  // myunion2.array[0] = array2[0];
  // myunion2.array[1] = array2[1];
  // myunion2.array[2] = array2[2];
  // myunion2.array[3] = array2[3];

  //array3안에 있는 7byte를 RES구조체에 복사한다
  memcpy(&RES2, array4, sizeof(RES2));

}

void loop() {
  // Serial.write(myunion.output,2);
  // Serial.println(myunion.input);
  //mystruct가 가진 주소를 byte*타입으로 형변환한다
  //Serial.write((byte*)(&mystruct),sizeof(mystruct));
  // Serial.write((byte*)(&MODbUS),sizeof(MODbUS));
  // Serial.println(RES.addr, HEX);
  // Serial.println(RES.fc, HEX);
  // Serial.println(RES.num, HEX);
  // Serial.println(RES.data, HEX); 


  int16_t temp = (RES2.tem_h<<8) | RES2.tem_l;
  int16_t humi = (RES2.humi_h<<8) | RES2.humi_l;
  Serial.print("temp = "); Serial.println(temp/10.0);
  Serial.print("humi = "); Serial.println(humi/10.0);
  Serial.print("crc = "); Serial.println((RES2.crc));
  delay(1000);
}

