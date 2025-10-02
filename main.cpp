#include <iostream>
#include <string>
#include <cstdint>
#include <math.h>
#include <bitset>
#include <map>
#include <limits>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;
uint16_t MAX_EXPS = 0xffu, MAX_EXPH = 0x1fu;
uint32_t MAX_MANTS = 0x7fffffu;
uint16_t MAX_MANH = 0x7ffu;

map<string, string> dhex = {
    {"0000", "0"}, {"0001", "1"}, {"0010", "2"}, {"0011", "3"}, {"0100", "4"}, {"0101", "5"}, {"0110", "6"},
    {"0111", "7"}, {"1000", "8"}, {"1001", "9"}, {"1010", "a"}, {"1011", "b"}, {"1100", "c"}, {"1101", "d"},
    {"1110", "e"}, {"1111", "f"}
};

int parseHex(char o) {
    if ('0' <= o && o <= '9') {
        return o - '0';
    }
    if ('A' <= o && o <= 'Z') {
        return o - 'A' + 10;
    }
    if ('a' <= o && o <= 'z') {
        return o - 'a' + 10;
    }
    return 0;
}

bool sings(uint32_t num) {
    return (num >> 31) & 0x1;
}

bool singh(uint16_t num) {
    return (num >> 15) & 0x1;
}

int exps(uint32_t num) {
    return static_cast<uint16_t>((num >> 23) & MAX_EXPS);
} // uint8_t не робит

uint16_t exph(uint16_t num) {
    return static_cast<uint16_t>((num >> 10) & MAX_EXPH);
} // uint8_t не робит

uint32_t mants(uint32_t num) {
    return num & MAX_MANTS;
}

uint16_t manth(uint16_t num) {
    return num & MAX_MANH;
}

void prints(uint32_t num) {
    cout << "sign: " << sings(num) << "\n" << "exp: " << exps(num) << "\n" << "mant: " << mants(num) << "\n";
} //helpful

void printh(uint16_t num) {
    cout << "sign: " << singh(num) << "\n" << "exp: " << exph(num) << "\n" << "mant: " << manth(num) << "\n";
} //helpful


void printS(bool f, uint16_t exp, uint32_t mant) {
    bitset<32> bs((f << 31) + (exp << 23) + mant);
    string s = "", a = "0x";
    for (int i = 31; i > -1; i -= 4) {
        s = "";
        for (int j = i; j > i - 4; j--) {
            s += std::to_string(bs[j]);
        }
        a += dhex[s];
    }
    cout << a << "\n";
    return;
}

void bitsets(string &n) {
    if (n.size() > 10) n = n.substr(n.size() - 10);
    uint32_t num = static_cast<uint32_t>(stoul(n, nullptr, 0));

    // s, exp, mant
    bool f = sings(num);
    int exp = exps(num);
    uint32_t mant = mants(num);
    prints(num);

    //NAN
    if (exp == MAX_EXPS && mant) {
        cout << "nan ";
        printS(f, exp, mant);
        return;
    }

    //INF
    if (exp == MAX_EXPH && !mant) {
        if (f) cout << "-";
        cout << "inf ";
        printS(f, exp, mant);
        return;
    }

    //exp == 0
    if (!exp) {
        if (!mant) {
            cout << "";
            return;
        }
        int sf = 0;
        mant <<= 9;
        while (((mant << sf) & 0x80000000u) == 0) {
            sf++;
        }
        mant <<= (sf + 1);
        bitset<32> mantb(mant);
        sf += 15;
        if (f) cout << '-';
        cout << "0x1.";
        string s = "";
        for (int i = 31; i > 8; i -= 4) {
            s = "";
            for (int j = i; j > i - 4; j--) {
                s += std::to_string(mantb[j]);
            }
            cout << dhex[s];
        }
        cout << "p-" << sf << " ";
        printS(sings(num), exps(num), mants(num));
        return;
    }

    // адекваты
    exp -= 127;
    if (f) cout << '-';
    cout << "0x1.";
    mant <<= 1;
    bitset<24> mantb(mant);
    string s = "";
    for (int i = 23; i > -1; i -= 4) {
        s = "";
        for (int j = i; j > i - 4; j--) {
            s += std::to_string(mantb[j]);
        }
        cout << dhex[s];
    }
    cout << 'p';
    if (exp < 0) {
        cout << exp << ' ';
    } else {
        cout << '+' << exp << ' ';
    }
    printS(sings(num), exps(num), mants(num));
}

void bitseth(string &num) {

}


int main_(int argc, char *argv[]) {
    if (argc == 4) {
        string fmt = argv[1], roundum = argv[2], num = argv[3];
        if (fmt == "s") {
            bitsets(num);
        }
        if (fmt == "h") {
            bitseth(num);
        }
    } else if (argc == 6) {
        string fmt = argv[1], roundum = argv[2], sign = argv[3], num1 = argv[4], num2 = argv[5];
    } else if (argc == 7) {
    }
    return 0;
}


//0001 0111 1 2 4 16 = 23 - 104 40 8
//0110 1000 инверсия всего кроме 0
