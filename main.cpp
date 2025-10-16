#include <iostream>
#include <string>
#include <cstdint>
#include <math.h>
#include <bitset>
#include <vector>
#include <fstream>
#include <iomanip>
#include <map>


using namespace std;

// a0 b1 c2 d3 e4 f5

struct FMT {
    int M, E, val; // mantissa exp value_of_exp
    uint32_t EXP_MASK, MANT_MASK, SING_MASK;

    FMT(string s) {
        if (s == "h") {
            this->M = 10;
            this->E = 5;
            this->val = 15;
            this->EXP_MASK = 0x1f;
            this->SING_MASK = 0x8000;
            this->MANT_MASK = 0x3ff;
        } else {
            this->M = 23;
            this->E = 8;
            this->val = 127;
            this->EXP_MASK = 0xff;
            this->SING_MASK = 0x80000000;
            this->MANT_MASK = 0x7fffff;
        }
    }

    int countBytes() {
        return (M + E + 1) / 4;
    }
};

struct Decode {
    bool sign, isNan, isInf, isZero, isDenorm;
    uint32_t m, e;

    Decode(uint32_t num, FMT F) {
        this->sign = num & F.SING_MASK;
        this->e = (num >> F.M) & F.EXP_MASK;
        this->m = num & F.MANT_MASK;
        this->isNan = ((e == F.EXP_MASK) && m);
        this->isInf = ((e == F.EXP_MASK) && !m);
        this->isZero = (!e && !m);
        this->isDenorm = (!e && m);
    }
};

int firstBit(uint32_t m) {
    int cnt = 0;
    while (m >> cnt) {
        cnt++;
    }
    return cnt;
}

string valueString(uint32_t num, FMT F) {
    Decode d = Decode(num, F);
    int exp = 0;
    uint32_t mant = d.m;

    if (d.isNan) return "nan";
    if (d.isInf) return d.sign ? "-inf" : "inf";
    if (d.isZero) {
        string mant(F.countBytes(), '0');
        return string(d.sign ? "-" : "") + "0x0." + mant + "p+0";
    }
    if (d.isDenorm) {
        int fb = F.M - firstBit(d.m);
        exp = fb + F.val;
        mant <<= (fb + 1);
        mant &= F.MANT_MASK;
        exp = -exp;
    } else {
        exp = d.e - F.val;
    }
    mant <<= ((F.M + 4) / 4 * 4 - F.M);
    ostringstream os;
    os << hex << setw((F.M + 4) / 4) << setfill('0') << mant;
    return string(d.sign ? "-" : "") + "0x1." + os.str() + "p" + string(exp >= 0 ? "+" : "") + to_string(exp);
}

string hexString(uint32_t num, FMT F) {
    int b = (F.M + F.E + 1) / 4;
    ostringstream os;
    os << "0x" << uppercase << hex << setw(b) << setfill('0') << num;
    return os.str();
}

uint32_t parseHexUseLowBits(string s, FMT F) {
    int bytes = F.countBytes();
    if (s.size() > 2 + bytes) s = "0x" + s.substr(s.size() - bytes, bytes);
    return static_cast<uint32_t>(stoul(s, nullptr, 0));
}

int main(int argc, char *argv[]) {
    FMT F = FMT(argv[1]);

    if (argc == 4) {
        uint32_t num = parseHexUseLowBits(argv[3], F);
        string val = valueString(num, F), h = hexString(num, F);
        cout << val << " " << h << "\n";
    }
    if (argc == 6) {
    }
    if (argc == 7) {
    }
    return 0;
}
