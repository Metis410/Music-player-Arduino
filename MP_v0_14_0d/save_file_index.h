//save setting .txt
// increment// 1 - 255 degred // 1 byte // 0   1
// 1 byte blank sapcing with | or 124 or 0x7C

// rotation speed // 1 to 180 degree per second // 1 byte // 2   3
// 1 byte blank

// rotate direction // bool // 1 byte // 4   5
// 1 byte blank

// fast scroll 1 // 1 to 255 // 2 byte // 6 7   8
// 1 byte blank

//fast scroll 2 // 1 to 255 // 2 byte // 9 10   11
// 1 byte blank

//fast scroll 3 // 1 to 255 // 2 byte // 12 13   14
// 1 byte blank

//LR balance // 2byte // 1 for uint8, 1 for L or R // 15 16   17
// 2 byte blank

//EQ // 9 byte // 19 - 27   28
// 1 byte blank



//sdinfo.txt
//0 - 1  //2bytes
//total album

//3 - 4 //2bytes
//songs amount

//6 //1byte
//total playlist

// 8 - 11 4bytes
// sd used size



//activetime.txt
//0 //uint64_t