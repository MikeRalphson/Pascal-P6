2 DIM N(20),M(20),A(8)
5 REM *** KENO WRITTEN BY VINCENT FAZIO, EDISON TECH HS, ROCHESTER, NY
6 REM *** CONVERTED TO BASIC-PLUS BY DAVE AHL, DIGITAL
9 PRINT :PRINT 
10 PRINT "KENO IS PLAYED IN MANY CASINOS IN LAS VEGAS."
19 PRINT 
20 PRINT "THE PLAYER CHOOSES 8 DIFFERENT NUMBERS FROM 1 TO 80,"
23 PRINT "INCLUSIVE, AND BETS $1.20. THE COMPUTER WILL SELECT"
26 PRINT "20 NUMBERS AT RANDOM AND WILL ELIMINATE DUPLICATES WHICH"
29 PRINT "MAY OCCUR. ANOTHER NUMBER WILL BE INSERTED IN ITS PLACE"
32 PRINT "SO THAT THE COMPUTER WILL OUTPUT 20 DIFFERENT NUMBERS."
35 PRINT :PRINT 
38 PRINT :PRINT :PRINT "HERE WE GO!!!"
40 PRINT "THE COMPUTER WILL OUTPUT A ? MARK. TYPE A NUMBER FROM "
43 PRINT "1 TO 80, INCLUSIVE, AND PRESS THE RETURN KEY. REPEAT THIS"
46 PRINT "PROCESS UNTIL THE ? MARK IS NO LONGER SHOWN."
48 FOR I=1 TO 8
50 INPUT A(I)
51 IF A(I)>80 GOTO 56 
53 IF A(I)=0 GOTO 56 
54 IF A(I)<0 GOTO 56 
55 GOTO 57 
56 PRINT "TYPE A NUMBER FROM 1 TO 80, INCLUSIVE, PLEASE.":GOTO 50 
57 NEXT I
58 C=0:FOR K=1 TO 7
59 FOR J=K TO 7
60 X=A(K):Y=A(J+1)
61 IF X=Y THEN 62 :GOTO 72 
62 C=C+1
63 PRINT "A DUPLICATE NUMBER HAS BEEN DETECTED IN YOUR INPUT."
64 PRINT "TYPE ANOTHER NUMBER, PLEASE.":INPUT Y
65 IF Y=0 GOTO 70 
66 IF Y>80 GOTO 70 
67 IF Y<0 GOTO 70 
68 A(J+1)=Y
69 GOTO 61 
70 PRINT "TYPE A NUMBER FROM 1 TO 80,INCLUSIVE, PLEASE."
71 GOTO 64 
72 NEXT J
73 NEXT K:PRINT :PRINT 
75 IF C=0 GOTO 88 
76 GOTO 58 
88 PRINT "THE COMPUTER WILL SELECT 20 NUMBERS AT RANDOM. THE BELL TONE"
91 PRINT "INDICATES IT IS IN THE PROCESS OF SELECTING THE NUMBERS."
97 RANDOMIZE
100 FOR L=1 TO 20
103 N(L)=INT(80*RND(0)+1)
106 M(L)=N(L)
109 NEXT L
110 L=20
112 FOR K=1 TO 20
115 FOR J=K TO L-1
118 X=M(K)
121 Y=M(J+1)
124 IF X<>YGOTO 139 
127 RANDOMIZE
130 M(J+1)=INT(80*RND(0)+1)
133 Y=M(J+1)
136 GOTO 124 
139 PRINT CHR$(7);
142 NEXT J
145 NEXT K
147 PRINT :PRINT 
148 PRINT "YOUR NUMBERS ARE:"
151 FOR I=1 TO 8
154 PRINT A(I);
157 NEXT I:PRINT :PRINT :PRINT 
160 PRINT "THE COMPUTER HAS SELECTED THE FOLLOWING NUMBERS:"
163 FOR L=1 TO 20
167 PRINT M(L);
170 NEXT L:PRINT :PRINT 
173 PRINT "THE PROGRAM WILL COMPARE YOUR NUMBERS WITH THE "
176 PRINT "NUMBERS THE COMPUTER HAS SELECTED."
179 PRINT :PRINT 
188 PRINT "YOU HAVE GUESSED THE FOLLOWING NUMBERS:"
191 G=0
194 FOR I=1 TO 8
197 FOR J=1 TO 20
200 X=A(I)
203 Y=M(J)
206 IF X=Y GOTO 213 
209 NEXT J
210 GOTO 225 
213 PRINT "CORRECT GUESS";
219 PRINT A(I);
222 G=G+1
225 NEXT I:PRINT 
228 IF G<5 GOTO 242 
231 IF G=5 GOTO 261 
234 IF G=6 GOTO 267 
237 IF G=7 GOTO 273 
240 IF G=8 GOTO 279 
242 PRINT "YOU CAUGHT";G;"NUMBER(S) OUT OF 8--"
243 PRINT "NOT ENOUGH CORRECT GUESSES--'SO SOLLY', NO PAYOFF."
245 PRINT :PRINT 
246 C=0
250 PRINT "WANT TO PLAY KENO AGAIN(YES OR NO)";:INPUT X$
252 IF X$="YES" GOTO 38 
253 IF X$="NO" GOTO 299 
254 C=C+1
255 IF C=3 GOTO 299 
256 PRINT "PLEASE TYPE 'YES' OR 'NO'":GOTO 250 
261 PRINT "YOU CAUGHT ";G;" NUMBERS OUT OF 8--YOU WIN $10.00"
264 GOTO 246 :PRINT :PRINT 
267 PRINT "YOU CAUGHT ";G;" NUMBERS OUT OF 8--YOU WIN $100.00"
270 GOTO 246 :PRINT :PRINT 
273 PRINT "YOU CAUGHT ";G;" NUMBERS OUT OF 8--YOU WIN $2,200.00"
276 GOTO 246 :PRINT :PRINT 
279 PRINT "YOU CAUGHT ";G;" NUMBERS OUT OF 8--YOU WIN $25,000.00"
282 PRINT "8 OUT OF 8 DOES NOT OCCUR TOO OFTEN,LUCKY."
285 GOTO 246 :PRINT :PRINT 
299 PRINT "THAT'S ALL FOR NOW.  PLAY KENO AGAIN !  BE SEEING YOU !"
300 END
