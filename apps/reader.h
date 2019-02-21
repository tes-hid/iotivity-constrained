#ifndef READER_H
#define READER_H

#define SAM_CSN { 0x44, 0x0A, 0x44, 0x00, 0x00, 0x00, 0xA0, 0x02, 0x96, 0x00 }
#define SAM_CSN_LEN 10

#ifdef __cplusplus
extern "C"
{
#endif

int reader_init(void);
void reader_register(void);

#ifdef __cplusplus
}
#endif

#endif
