#include <iostream>
//#include <iomanip>

using namespace std;


/* SETTINGS	------------------------------------------------------------------*/

// CHOOSE A TYPE OF DYSPLAY //
#define SEVEN_SEGMENT_DYSPLAY
//#define SIXTEEN_SEGMENT_DYSPLAY

// TYPE A QUANTITY OF DYSPLAYS //
#define NUMBER_OF_DYSPLAYS 4

// CHOOSE AN OPPORTUNITY TO USE ENGLISH ALPHABET //
//#define TURN_ON_ENG_ALPHABET

// CHOOSE A PORT FOR CONNET TO SEGMENT DISPLAY //
//#define PORT_B
#ifndef PORT_B
#define PORT_A
#endif

// CHOOSE A LOGICAL LEVEL OF PRESSED BUTTON //
//#define BUTTON_GND
#ifndef BUTTON_GND  //low-level
#define BUTTON_VDD  //high-level
#endif

// SET A FLAG FOR DECLARING DEBOUNCER TURN IT ON //
//#define TURN_ON_CONTACT_DEBOUNCER

// IT'S FLAG NEED FOR ENCODE A HEXADECIMAL PORT VALUE IN DECIMAL //
#define ENCODE_HEX_TO_DEC

// THIS FLAG NEED FOR READ ODR REGISTER OF MCU //
//#define BUTTON_READ_STATE LL_GPIO_IsInputPinSet(GPIOA, BUTTON)

#define TIMEOUT     640000
#define DELAY       120000
#define LOOPS       2

#ifdef TURN_ON_CONTACT_DEBOUNCER
    #define TEN_MS      80
#endif
#ifdef TURN_ON_ENG_ALPHABET
    #define STR_LENGTH  16
#endif

/* END SETTINGS	--------------------------------------------------------------*/


/*
  ***************************************************************************
  *                                                                         *
  * (16bit)   0 x 0000 0000 0000 0000   (LOW Part of BSR Register)          *
  *                   ^^^^ ^ ^^^ ^^^^                                       *
  * PORT B    Rank 4,3,2,1 DP DECODER     --> MASK = 0xfff                  *
  *                                                                         *
  ***************************************************************************
*/
static       bool       selected_loop   = false;
static       uint16_t   program_state   = 0;
static       uint16_t   ranks           = 0;
static       uint16_t   cnt_number      = 0;
static       uint16_t   numBuffer       = 0;
static       uint16_t   numBufferMax    = 50;
static       uint16_t   mask            = 0xfff;
//static const uint16_t   DOT_MASK        = 0x80;

#ifndef SIXTEEN_SEGMENT_DYSPLAY
    static const uint16_t	RANK_SELECTOR[] = {0x100, 0x200, 0x400, 0x800};
#elif
    static const uint16_t	RANK_SELECTOR[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20};
#endif
static const uint16_t   DECIMAL_NUMBERS_DECODER[] =
    {
        0x3f, 0x06, 0x5b, 0x4f, 0x66,
        0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };
static const uint32_t   MAX_RANKS       = sizeof(RANK_SELECTOR)             /sizeof (uint16_t);
static const uint32_t   MAX_NUMBERS     = sizeof(DECIMAL_NUMBERS_DECODER)   /sizeof (uint16_t);

enum PROGRAM_STATE
{
    CHOSE_A_STATE, ENTER_NUM, LOOP_NUM, COUNT_NUM
};

#if defined (TURN_ON_ENG_ALPHABET)
static enum ALPHABET_DECODER
    {
        A = 0x77U, B = 0x7cU, C = 0x58U, D = 0x5eU,
        E = 0x79U, F = 0x71U, G = 0x3dU, H = 0x74U,
        I = 0x30U, J = 0x1eU, K = 0x75U, L = 0x38U,
        M = 0x15U, N = 0x54U, O = 0x5cU, P = 0x73U,
        Q = 0x5cU, R = 0x50U, S = 0x2dU, T = 0x78U,
        U = 0x3eU, V = 0x1cU, W = 0x2aU, X = 0x76U,
        Y = 0x6eU, Z = 0x1bU,

        a = 0x77U, b = 0x7cU, c = 0x58U, d = 0x5eU,
        e = 0x79U, f = 0x71U, g = 0x3dU, h = 0x74U,
        i = 0x30U, j = 0x1eU, k = 0x75U, l = 0x38U,
        m = 0x15U, n = 0x54U, o = 0x5cU, p = 0x73U,
        q = 0x5cU, r = 0x50U, s = 0x2dU, t = 0x78U,
        u = 0x3eU, v = 0x1cU, w = 0x2aU, x = 0x76U,
        y = 0x6eU, z = 0x1bU

    } letter;
#endif

uint32_t get_number = 0;
uint32_t port_state = 0;
uint32_t output     = 0;
#if defined(TURN_ON_CONTACT_DEBOUNCER)
    uint32_t debouncer      = 0;
    uint32_t button_pressed = 0;
#endif

// CONVERT IN BINARY FORMAT FUNCTION BEGIN //
 void convert_to_binary(uint32_t x)
 {
    int i = 16;             // 16 means the range of printed bits
    while (i)
    {
        if (i % 4 == 0)
            cout << " ";    // split into groups of four
        cout << ((x & 0x8000) >> 15);
        i--;
        x <<= 1;            // left binary shift by 1 (divide by 2)
    }
}
// CONVERT IN BINARY FORMAT FUNCTION END //


// PROGRAMM DELAY FUNCTION BEGIN //
static void delay(uint32_t b)
{
    for(uint32_t a = 0; a < b; a++);
}
// PROGRAMM DELAY FUNCTION END//

// SET INDICATOR FUNCTION BEGIN //
static void set_indicator(  uint16_t port_value,
                            uint16_t rank)
{
    // In this case, we don't need to use it, but in other - instead port_value
    // we must read an ODR register value for get a real port state.
    // port_value (change on) -> LL_GPIO_ReadOutputPort(GPIOA)
    port_state = port_value;
/*
    ******************************************************************
    * LOGIC:                                                         *
    * get a port_state like 0x 0000 0000 0101 1011                   *
    * mask = 0x 0000 1111 1111 1111 (allowed change bits)            *
    * ~mask = 0x 1111 0000 0000 0000 (invert for set remain bits)   *
    * result = result & ~mask                                        *
    * result = result \ port_state (modify 7-bits 0101 1011)         *
    * result -> 0x 0000 0000 0101 1011                               *
    ******************************************************************
*/
    output = (port_state & ~mask)
                | RANK_SELECTOR             [rank       % MAX_RANKS]
                | DECIMAL_NUMBERS_DECODER   [port_value % MAX_NUMBERS];
#ifndef ENCODE_HEX_TO_DEC
    cout << "There's a Port value for MCU -> "  << output << endl;
    delay(TIMEOUT);
#else
// EXAMPLE of using ENCODER --------------------------------------------------------- //
// In this case, we don't need to use an Encoder, but in other - it might need.
    uint16_t decoded_port_value =  DECIMAL_NUMBERS_DECODER[port_value % MAX_NUMBERS];
    for (uint16_t i = 0; i < MAX_NUMBERS; i++)
    {
        if(DECIMAL_NUMBERS_DECODER[i] ==  decoded_port_value)
        {
            cout << "\nPort value for MCU (hexadecimal format)\t -> 0x  0000 0000 0000 0" << hex << output;
            cout << "\nPort value for MCU (binary format)\t -> 0b ";
            convert_to_binary(output);
            cout << "\nPort value for USER \t\t\t -> 0d  " << i << endl;
            delay(TIMEOUT);
            break;
        }
    }
#endif
// EXAMPLE of using ENCODER END ----------------------------------------------------- //
}
// SET INDICATOR FUNCTION END //


// SPLIT FUNCTION BEGIN //
void splitNumber_byRank(uint16_t get_value)
{
    uint16_t ranks  = 0;
    numBuffer = get_value;
    do
    {
        uint32_t tmp = numBuffer % 10;
        set_indicator(tmp, ranks);
        numBuffer /= 10;
        if(ranks < MAX_RANKS)
            ranks++;
        else
            ranks = 0;
    }
    while (numBuffer > 0);
    ranks = 0;
}
// SPLIT FUNCTION END //


#ifdef TURN_ON_CONTACT_DEBOUNCER
// DEBOUNCER FUNCTION BEGIN //
    void debounser()
    {
        if(get_number == 5)
        {
            if(debouncer < TEN_MS)
                debouncer++;
            else
            {
                if(button_pressed == 0)
                {
                    button_pressed = 1;
                    cout << "Button \"C\" was pressed!" << endl;
                }
            }
        }
        else
        {
            if(debouncer > 0)
                debouncer--;
            else
                button_pressed = 0;
        }
    }
// DEBOUNCER FUNCTION END //
#endif


int main()
{
    while (1)
    {
        switch (program_state)
        {
            case (CHOSE_A_STATE): // User enters a number
                {
                    cout << "Hello, Dear User! :)\n" << endl;
                    cout << "Please, decide on the status of the program:" << endl;
                    cout << "\tType - 1 [Enter] to enter a number and display it dynamically," << endl;
                    cout << "\tType - 2 [Enter] to dynamically display a number in a loop," << endl;
                    cout << "\tType - 3 [Enter] to activate a counter from 0 to "<< LOOPS <<" that dynamically displays each number." << endl;
                    cout << "What will you choose?" << endl;
                    cin >> program_state;
                    if (cin.get() == (uint32_t)'\n')
                    {
                        switch (program_state)
                        {
                            case ENTER_NUM: 
                                    program_state = ENTER_NUM;
                                break;
                            case LOOP_NUM:
                                    program_state = LOOP_NUM;
                                break;
                            case COUNT_NUM:
                                    program_state = COUNT_NUM;
                                break;
                            default:
                                {
                                    cout << "\n\n\nERROR!!!---ERROR!!!---ERROR!!!---ERROR!!!---ERROR!!!---ERROR!!!\n\n";
                                    cout << "\tERROR!!! You made a mistake while entering number!\n";
                                    cout << "\tCould you please enter a number from 1 to 3.\n";
                                    cout << "\tThanks for your understanding!" << endl;
                                    program_state = CHOSE_A_STATE;
                                }
                                break;
                        }
                    }
                    else
                    {
                        cout << "\n\n\nERROR!!!---ERROR!!!---ERROR!!!---ERROR!!!---ERROR!!!---ERROR!!!\n\n";
                        cout << "\tERROR!!! You made a mistake while entering number!\n";
                        cout << "\tCould you please enter a number from 1 to 3.\n";
                        cout << "\tThanks for your understanding!" << endl;
                        program_state = CHOSE_A_STATE;
                    }
                }
                break;
            case (ENTER_NUM): // User enters a number
                {
                    cout << "Please, enter your integer number from 0 to 65535:" << endl;
                    cin >> get_number;
                    if (cin.get() == (uint32_t) '\n')
                    {
                        if(selected_loop == true)
                            program_state = LOOP_NUM;
                        else
                        {
                            splitNumber_byRank(get_number);
                            program_state = CHOSE_A_STATE;
                        }
                    }
                    else
                    {
                        cout << "ERROR!!! You made something wrong! Please, try again :" << endl;
                        program_state = ENTER_NUM;
                    }
                }
                break;
            case (LOOP_NUM): // Outputting user number in a loop
                {
                    if (!get_number)
                    {
                        selected_loop = true;
                        program_state = ENTER_NUM;
                        break;
                    }
                    int counter = LOOPS;
                    cout << "\n\nOutput number in loop start here: " << endl;
                    do
                    {
                        splitNumber_byRank(get_number);
                        counter--;
                    }
                    while (counter);
                    cout << "\nThere's the end output number in loop. \n\n" << endl;
                    selected_loop = false;
                    program_state = COUNT_NUM;
                }
                break;
            case (COUNT_NUM): // Outputting counter from 0 to 500
                {
                    while(cnt_number < numBufferMax)
                    {
                        splitNumber_byRank(cnt_number);
                        cnt_number++;
                    }
                    program_state = ENTER_NUM;
                    cnt_number = 0;
                }
                break;
#if defined (TURN_ON_ENG_ALPHABET)
           case 3:
                {
                    char str[STR_LENGTH];
                    cout << "Enter your string (Max: 16 symbols): " << endl;
                    cin.get(str, STR_LENGTH);
                    for (int i = 0; i < STR_LENGTH; i++)
                    {
                        if(str[i] != ' ' && str[i] != '\0')
                        {
                            char symbol = str[i];
                            uint32_t alphabet = ALPHABET_DECODER::H;
                            set_indicator(alphabet, ranks);
                    }
                }
                break;
#endif
           default: program_state = ENTER_NUM; break;
        };
    }
    return 0;
}
