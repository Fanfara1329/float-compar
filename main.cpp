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

    int countBytes() { return (M + E + 1) / 4; }
    int mantBytes() { return (M + 4) / 4; }

    uint32_t qNan() {
        return (SING_MASK | (EXP_MASK << M) | (1u << (M - 1)));
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

struct EndPack {
    uint64_t mant;
    int e;
    bool sign;
    bool isZero;

    EndPack(uint64_t sig, int e, bool sign, bool isZero) {
        this->mant = sig;
        this->e = e;
        this->sign = sign;
        this->isZero = isZero;
    }
};

int firstBit32(uint32_t m) {
    int cnt = 0;
    while (m >> cnt) { cnt++; }
    return cnt;
}

int firstBit64(uint64_t m) {
    int cnt = 0;
    while (m >> cnt) { cnt++; }
    return cnt;
}

void buildME(Decode d, int &exp, uint64_t &mant, FMT F) {
    if (d.isZero) {
        exp = 0;
        mant = 1 - F.val;
        return;
    }

    // вот это не нужно тк все скипается в mult
    if (d.isDenorm) {
        mant = (uint64_t) d.m;
        exp = 1 - F.val;
    } else {
        mant = (1u << F.M) | d.m;
        exp = (int) d.e - F.val;
    }
}

void shift(uint64_t &m, int sh, bool &lost, bool &g, bool &t) {
    uint64_t mask = (1ull << sh) - 1, n = ((m & mask) ? 1ull : 0ull);
    lost |= n;
    g = ((m >> (sh - 1)) & 1ull) != 0;
    t = (n & ((1ull << (sh - 1)) - 1ull)) != 0;
    m >>= sh; //| ((m & mask) ? 1ull : 0ull);
}

string valueString(uint32_t num, FMT F) {
    Decode d = Decode(num, F);
    int exp = 0;
    uint32_t mant = d.m;
    if (d.isNan) return "nan";
    if (d.isInf) return d.sign ? "-inf" : "inf";
    if (d.isZero) {
        string mant(F.mantBytes(), '0');
        return string(d.sign ? "-" : "") + "0x0." + mant + "p+0";
    }
    if (d.isDenorm) {
        int fb = F.M - firstBit32(d.m);
        exp = fb + F.val;
        mant <<= (fb + 1);
        mant &= F.MANT_MASK;
        exp = -exp;
    } else { exp = d.e - F.val; }
    mant <<= ((F.M + 4) / 4 * 4 - F.M);
    ostringstream os;
    os << hex << setw(F.mantBytes()) << setfill('0') << mant;
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

uint32_t pack(bool sign, uint32_t e, uint32_t m, FMT F) {
    uint32_t num = ((e & F.EXP_MASK) << F.M) | (m & F.MANT_MASK);
    if (sign) num |= F.SING_MASK;
    return num;
}

uint32_t quietNaN(Decode num, const FMT &F) {
    uint32_t frac = num.m | (1u << (F.M - 1));
    return pack(num.sign, F.EXP_MASK, frac, F);
}

static uint32_t finishPack(EndPack &P, FMT &F, string s) {
    if (P.isZero || !P.mant) return pack(P.sign, 0, 0, F);

    bool lost = false;
    bool g = false, t = false;
    int fr = firstBit64(P.mant) - 1;
    if (fr > F.M) {
        shift(P.mant, fr - F.M, lost, g, t);
        P.e += (fr - F.M);
    }

    while (firstBit64(P.mant) - 1 < F.M && P.e > (1 - F.val)) {
        P.mant <<= 1;
        P.e--;
    }
    if (P.e <= (1 - F.val)) {
        shift(P.mant, (1 - F.val) - P.e, lost, g, t);
        P.e = (1 - F.val);
    }
    int fb = firstBit64(P.mant), sh = max(0, fb - F.M - 1);
    uint64_t kept = P.mant >> sh, r = P.mant & ((1ull << sh) - 1);
    int rm = (s == "0" ? 0 : (s == "1" ? 1 : (s == "2" ? 2 : 3)));

    bool inexact = (sh > 0) ? (r != 0) : lost;
    bool inc = false;
    //cout << g << "  " << t << "   " << (kept&1ull) << "\n";
    if (rm == 2) {
        inc = (!P.sign) && inexact; // toward +inf
    } else if (rm == 3) {
        inc = (P.sign) && inexact; // toward -inf
    } else if (rm == 1) {
        if (sh > 0) {
            cout << "hbgvfcd\n";
            uint64_t half = 1ull << (sh - 1);
            if (r > half) inc = true;
            else if (r == half) inc = (kept & 1ull); // tie -> to even
            else inc = false;
        } else if (sh == 0) {
            if (g) {
                if (t) inc = true;
                else if (kept & 1ull) kept++;
            }
        }
    }

    if (inc) kept++;

    while (firstBit64(kept) > F.M + 1) {
        kept >>= 1;
        P.e++;
    }
    if (P.e + F.val >= F.EXP_MASK) {
        if (s == "1" || (s == "2" && !P.sign) || (s == "3" && P.sign)) return pack(P.sign, F.EXP_MASK, 0, F);
        return pack(P.sign, F.EXP_MASK - 1, F.MANT_MASK, F);
    }
    uint32_t ans = (uint32_t) (kept & F.MANT_MASK);
    if (P.e > 1 - F.val) {
        return pack(P.sign, (uint32_t) (P.e + F.val), ans, F);
    }
    return pack(P.sign, 0, ans, F);
}

uint32_t mult(uint32_t A, uint32_t B, FMT F, string s) {
    Decode a = Decode(A, F), b = Decode(B, F);
    if (a.isNan) return quietNaN(a, F);
    if (b.isNan) return quietNaN(b, F);
    if ((a.isInf && b.isZero) || (b.isInf && a.isZero)) return F.qNan();
    bool sign = a.sign ^ b.sign;
    if (a.isInf || b.isInf) return pack(sign, F.EXP_MASK, 0, F);
    if (a.isZero || b.isZero) return pack(sign, 0, 0, F);
    int exp_a, exp_b;
    uint64_t mant_a, mant_b;
    buildME(a, exp_a, mant_a, F);
    buildME(b, exp_b, mant_b, F);
    uint64_t comp = mant_a * mant_b;
    int exp = exp_a + exp_b - F.M;
    EndPack P{comp, exp, sign, false};

    return finishPack(P, F, s);
}

uint32_t div(uint32_t A, uint32_t B, FMT F, string s) {
    Decode a = Decode(A, F), b = Decode(B, F);
    if (a.isNan) return A;
    if (b.isNan) return B;
    if ((a.isInf && b.isZero) || (b.isInf && a.isZero)) return F.qNan();
    bool sign = a.sign ^ b.sign;
    if (a.isInf || b.isZero) return pack(sign, F.EXP_MASK, 0, F);
    if (a.isZero || b.isInf) return pack(sign, 0, 0, F);
    int exp_a, exp_b;
    uint64_t mant_a, mant_b;
    buildME(a, exp_a, mant_a, F);
    buildME(b, exp_b, mant_b, F);
    uint64_t num = ((F.M + 2) >= 64) ? 0ull : ((uint64_t) mant_a << (F.M + 2));
    uint64_t q = num / mant_b;
    uint64_t r = num % mant_b;
    if (r) q |= 1ull;
    EndPack P{q, exp_a + exp_b - 2, sign, (q == 0)};
    return finishPack(P, F, s);
}

uint32_t addsub(uint32_t A, uint32_t B, FMT F, string s, int p) {
    Decode a = Decode(A, F), b = Decode(B, F);
    if (a.isNan) return A;
    if (b.isNan) return B;
    if ((a.isInf && b.isZero) || (b.isInf && a.isZero)) return F.qNan();
    bool sign = a.sign ^ b.sign;
    if (a.isInf || b.isInf) return pack(sign, F.EXP_MASK, 0, F);
    if (a.isZero || b.isZero) return pack(sign, 0, 0, F);
    int exp_a, exp_b;
    uint64_t mant_a, mant_b;
    buildME(a, exp_a, mant_a, F);
    buildME(b, exp_b, mant_b, F);
    uint64_t comp = mant_a * mant_b;
    int exp = exp_a + exp_b;
    EndPack P{comp, exp, sign, false};
    return finishPack(P, F, s);
}

int main(int argc, char *argv[]) {
    FMT F = FMT(argv[1]);
    if (argc == 4) {
        uint32_t num = parseHexUseLowBits(argv[3], F);
        string val = valueString(num, F), h = hexString(num, F);
        cout << val << " " << h << "\n";
    }
    if (argc == 6) {
        string op = argv[3];


        uint32_t a = parseHexUseLowBits(argv[4], F), b = parseHexUseLowBits(argv[5], F), n = 0;
        if (op == "!") n = mult(a, b, F, argv[2]);
        if (op == "/") n = div(a, b, F, argv[2]);
        if (op == "+") n = addsub(a, b, F, argv[2], 1);
        if (op == "-") n = addsub(a, b, F, argv[2], 0);
        string val = valueString(n, F), h = hexString(n, F);
        cout << val << " " << h << "\n";
    }
    if (argc == 7) {
    }
    return 0;
}


// h 1 ! 0x67c 0x29f7
//11  1444   first1
//4  5   first2
//1000  0101


// h 1 ! 0x9a8f 0xa219
//11  1527   first1
//1  1   first2
//
