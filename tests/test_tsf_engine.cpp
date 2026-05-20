// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Moab

#include "engine.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

static void feed_utf8(CEthiopicTextService *svc, const char *key)
{
    svc->ProcessKeyUtf8(key);
}

static std::string committed(CEthiopicTextService *svc)
{
    return svc->TestCommittedText();
}

static std::string preedit(CEthiopicTextService *svc)
{
    return svc->TestPreeditText();
}

int main()
{
    auto *svc = new CEthiopicTextService();
    svc->SetTestMode(true);

    // Test 1: Single key shows pending in preedit
    {
        feed_utf8(svc, "h");
        assert(committed(svc).empty());
        assert(preedit(svc) == "\xe1\x88\x85");   // ህ = U+1205
        std::cout << "  PASS: 'h' -> preedit 'ህ'\n";
        svc->ResetTest();
    }

    // Test 2: Leaf auto-commit "he" -> "ሀ"
    {
        feed_utf8(svc, "h");
        feed_utf8(svc, "e");
        assert(committed(svc) == "\xe1\x88\x80");   // ሀ = U+1200
        assert(preedit(svc).empty());
        std::cout << "  PASS: 'he' -> commit 'ሀ'\n";
        svc->ResetTest();
    }

    // Test 3: Branch preedit "hi" -> preedit "ሂ"
    {
        feed_utf8(svc, "h");
        feed_utf8(svc, "i");
        assert(committed(svc).empty());
        assert(preedit(svc) == "\xe1\x88\x82");   // ሂ = U+1202
        std::cout << "  PASS: 'hi' -> preedit 'ሂ'\n";
        svc->ResetTest();
    }

    // Test 4: "hie" auto-commits "ሄ"
    {
        feed_utf8(svc, "h");
        feed_utf8(svc, "i");
        feed_utf8(svc, "e");
        assert(committed(svc) == "\xe1\x88\x84");   // ሄ = U+1204
        assert(preedit(svc).empty());
        std::cout << "  PASS: 'hie' -> commit 'ሄ'\n";
        svc->ResetTest();
    }

    // Test 5: Multi-syllable word "selam" -> "ሰላ"
    {
        feed_utf8(svc, "s");
        feed_utf8(svc, "e");
        feed_utf8(svc, "l");
        feed_utf8(svc, "a");
        feed_utf8(svc, "m");
        assert(committed(svc) == "\xe1\x88\xb0\xe1\x88\x8b");  // ሰላ (6 bytes)
        assert(preedit(svc) == "\xe1\x88\x9d");   // ም pending (3 bytes)
        std::cout << "  PASS: 'selam' -> commit 'ሰላ' + preedit 'ም'\n";
        svc->ResetTest();
    }

    // Test 5b: Terminate "selam" with space -> "ሰላም "
    {
        feed_utf8(svc, "s");
        feed_utf8(svc, "e");
        feed_utf8(svc, "l");
        feed_utf8(svc, "a");
        feed_utf8(svc, "m");
        feed_utf8(svc, " ");
        assert(committed(svc) == "\xe1\x88\xb0\xe1\x88\x8b\xe1\x88\x9d ");  // ሰላም
        assert(preedit(svc).empty());
        std::cout << "  PASS: 'selam ' -> commit 'ሰላም '\n";
        svc->ResetTest();
    }

    // Test 6: Unmapped ASCII appended "#" -> "#"
    {
        feed_utf8(svc, "#");
        assert(committed(svc) == "#");
        assert(preedit(svc).empty());
        std::cout << "  PASS: '#' -> commit '#'\n";
        svc->ResetTest();
    }

    // Test 7: Mapped + unmapped "hi#" -> "ሂ#"
    {
        feed_utf8(svc, "h");
        feed_utf8(svc, "i");
        feed_utf8(svc, "#");
        assert(preedit(svc).empty());
        assert(committed(svc).size() == 4);  // 3 bytes + '#'
        std::cout << "  PASS: 'hi#' -> commit 4 bytes (ሂ + #)\n";
        svc->ResetTest();
    }

    // Test 8: Space word boundary
    {
        feed_utf8(svc, "h");
        feed_utf8(svc, "e");
        feed_utf8(svc, " ");
        std::string out = committed(svc);
        assert(out.size() == 4);   // 3-byte Ethiopic + space
        assert(out[3] == ' ');
        assert(preedit(svc).empty());
        std::cout << "  PASS: 'he ' -> commit 'ሀ '\n";
        svc->ResetTest();
    }

    // Test 9: Labiovelar "hWa" -> "ኋ"
    {
        feed_utf8(svc, "h");
        feed_utf8(svc, "W");
        feed_utf8(svc, "a");
        assert(committed(svc) == "\xe1\x8a\x8b");   // ኋ = U+128B
        assert(preedit(svc).empty());
        std::cout << "  PASS: 'hWa' -> commit 'ኋ'\n";
        svc->ResetTest();
    }

    // Test 10: 5th order "bE" vs "be"
    {
        feed_utf8(svc, "b");
        feed_utf8(svc, "e");
        assert(committed(svc) == "\xe1\x89\xa0");   // በ = U+1260
        svc->ResetTest();

        feed_utf8(svc, "b");
        feed_utf8(svc, "E");
        assert(committed(svc) == "\xe1\x89\xa4");   // ቤ = U+1264
        std::cout << "  PASS: 'be'='በ', 'bE'='ቤ'\n";
        svc->ResetTest();
    }

    // Test 11: Multiple syllables accumulate
    {
        feed_utf8(svc, "h"); feed_utf8(svc, "e"); feed_utf8(svc, " ");
        feed_utf8(svc, "l"); feed_utf8(svc, "e"); feed_utf8(svc, " ");
        feed_utf8(svc, "b"); feed_utf8(svc, "e"); feed_utf8(svc, " ");
        assert(committed(svc).size() == 12);  // 3 × (3-byte Ethiopic + space) = 12
        std::cout << "  PASS: 'he le be ' -> 12 bytes committed\n";
        svc->ResetTest();
    }

    // Test 12: Reset clears state
    {
        feed_utf8(svc, "h");
        feed_utf8(svc, "i");
        assert(!preedit(svc).empty());
        svc->ResetTest();
        assert(committed(svc).empty());
        assert(preedit(svc).empty());
        std::cout << "  PASS: ResetTest clears committed and preedit\n";
    }

    // Test 13: Slash prefix "/he" -> "ኀ"
    {
        feed_utf8(svc, "/");
        feed_utf8(svc, "h");
        feed_utf8(svc, "e");
        assert(committed(svc) == "\xe1\x8a\x80");   // ኀ = U+1280
        std::cout << "  PASS: '/he' -> commit 'ኀ'\n";
        svc->ResetTest();
    }

    // Test 14: Punctuation ":" -> "፡"
    {
        feed_utf8(svc, ":");
        assert(preedit(svc) == "\xe1\x8d\xa1");   // ፡ = U+1361
        std::cout << "  PASS: ':' -> preedit '፡'\n";
        svc->ResetTest();
    }

    // Test 15: Numeral "`10" -> preedit "፲" (branch node)
    {
        feed_utf8(svc, "`");
        feed_utf8(svc, "1");
        feed_utf8(svc, "0");
        assert(committed(svc).empty());
        assert(preedit(svc) == "\xe1\x8d\xb2");   // ፲ = U+1372
        std::cout << "  PASS: '`10' -> preedit '፲'\n";
        svc->ResetTest();
    }

    // Test 16: Standalone vowel "a" -> "አ"
    {
        feed_utf8(svc, "a");
        assert(preedit(svc) == "\xe1\x8a\xa0");   // አ = U+12A0
        std::cout << "  PASS: 'a' -> preedit 'አ'\n";
        svc->ResetTest();
    }

    // Test 17: UTF-8 byte integrity — committed "ሰላ" = 6 bytes, preedit "ም" = 3 bytes
    {
        feed_utf8(svc, "s"); feed_utf8(svc, "e");
        feed_utf8(svc, "l"); feed_utf8(svc, "a");
        feed_utf8(svc, "m");
        std::string out = committed(svc);
        assert(out.size() == 6);    // ሰላ = 2 × 3 bytes
        assert((unsigned char)out[0] == 0xE1);
        assert((unsigned char)out[3] == 0xE1);
        std::string p = preedit(svc);
        assert(p.size() == 3);      // ም = 3 bytes
        assert((unsigned char)p[0] == 0xE1);
        std::cout << "  PASS: UTF-8 integrity — 6 bytes committed, 3 preedit, all 0xE1\n";
        svc->ResetTest();
    }

    delete svc;
    std::cout << "\nAll TSF engine tests passed.\n";
    return 0;
}
