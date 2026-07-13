REM Tests SWAP statement
a%=10
b%=20
SWAP a%,b%
IF a%=20 AND b%=10 THEN
  PRINT "Test 1 (SWAP long): OK"
ELSE
  PRINT "Test 1 (SWAP long): FAILED"
ENDIF
c$="hello"
d$="world"
SWAP c$,d$
IF c$="world" AND d$="hello" THEN
  PRINT "Test 2 (SWAP string): OK"
ELSE
  PRINT "Test 2 (SWAP string): FAILED"
ENDIF
