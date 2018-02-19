/* List of supported ISO predicates */
:- op(50,  xfx, :).
:- op(100, xfx, @).
:- op(200, fy, '\\').
:- op(200, fy,  -).
:- op(200, xfy, ^).
:- op(200, xfx, **).
:- op(400, yfx, *).
:- op(400, yfx, /).
:- op(400, yfx, //).
:- op(400, yfx, rem).
:- op(400, yfx, mod).
:- op(400, yfx, <<).
:- op(400, yfx, >>).
:- op(500, yfx,  +).
:- op(500, yfx, '-' ).
:- op(500, yfx, '/\\').
:- op(500, yfx, '\\/').
:- op(700, xfx, =).
:- op(700, xfx, '\\=').
:- op(700, xfx, ==).
:- op(700, xfx, '\\==').
:- op(700, xfx, @<).
:- op(700, xfx, @=<).
:- op(700, xfx, @>).
:- op(700, xfx, @>=).
:- op(700, xfx, is).
:- op(700, xfx, =:=).
:- op(700, xfx, '=\\=').
:- op(700, xfx, <).
:- op(700, xfx, =<).
:- op(700, xfx, >).
:- op(700, xfx, >=).
:- op(700, xfx, =..).
:- op(900, fy, '\\+').
:- op(1000, xfx, ',').
:- op(1050, xfy, ->).
:- op(1100, xfy, ';').
:- op(1200, fx, ':-').
:- op(1200, xfx, ':-').
:- op(1200, xfx, -->).
A=A.
member(X, [X | Tail]).
member(X, [Head | Tail]) :- member(X, Tail).
one_member(X,[X|L]) :- !.
one_member(X,[Y|L]) :- member(X,L).
conc([],L,L).
conc([X|L1], L2, [X|L3]) :- conc(L1, L2, L3).
del(X, [X | Tail], Tail).
del(X, [Y | Tail], [Y | Tail1]) :- del(X, Tail, Tail1).
sublist(S, L) :- conc(L1, L2, L), conc(S, L3, L2).
insert(X, List, BiggerList) :- del(X, BiggerList, List).
permutation([], []).
permutation([X|L],P) :- permutation(L,L1), insert(X, L1, P).
length([], 0).
length([_|Tail], N) :- length(Tail, N1), N is 1 + N1.
max(X,Y,X) :- X>=Y,!.
max(X,Y,Y).
true.
different(X, X) :- !, fail.
different(X, Y).
not(P) :- P,!,fail.
not(P) :- true.
\+ P :- not(P).
atom_length(Atom,Integer):-atom_chars(Atom,C), length(C,Integer).
atom_concat(Atom1,Atom2,Atom3):-atom_chars(Atom1,C1),atom_chars(Atom2,C2),conc(C1,C2,C3),atom_chars(Atom3,C3).
drop([], N, []).
drop(A, 0, A).
drop([H|T], N, T2) :- N > 0, N1 is N - 1, drop(T, N1, T2).
take([], N, []).
take(A, 0, []).
take([H|T], N, [H|T2]) :- N > 0, N1 is N - 1, take(T, N1, T2).
droptake(A, N, L, S) :- drop(A, N, A1), take(A1, L, S).
sub_atom(A, N, L, S) :- atom_chars(A,C), droptake(C,N,L,C1), atom_chars(S,C1).
chars_codes([], []).
chars_codes([H|T], [CH|CT]) :- char_code(H,CH), chars_codes(T,CT).
atom_codes(A, C) :- atom_chars(A, C1), chars_codes(C1, C).
append([], A, [A]).
append([H|T], A, [H|T1]) :- append(T, A, T1).
number_codes(0,[48]).
number_codes(N, S) :- N < 10, N > 0, N1 is 48 + N, append([], N1, S).
number_codes(N,S2) :-
	N >= 10,
	N1 is N // 10,
	R is 48 + rem(N, 10),
	number_codes(N1, S1),
	append(S1, R, S2).
number_codes(N,[45|S]) :- N < 0, N1 is -N, number_codes(N1, S).
number_chars(N,Chars) :- number_codes(N,C), chars_codes(Chars,C).
if(A,B,C) :- A, B.
if(A,B,C) :- \+ A, C.
char_code('\a',7).
char_code('\b',8).
char_code('\t',9).
char_code('\n',10).
char_code('\v',11).
char_code('\f',12).
char_code('\r',13).
char_code('!',33).
char_code('"',34).
char_code('#',35).
char_code('$',36).
char_code('%',37).
char_code('&',38).
char_code('\'',39).
char_code('(',40).
char_code(')',41).
char_code('*',42).
char_code('+',43).
char_code(',',44).
char_code('-',45).
char_code('.',46).
char_code('/',47).
char_code('0',48).
char_code('1',49).
char_code('2',50).
char_code('3',51).
char_code('4',52).
char_code('5',53).
char_code('6',54).
char_code('7',55).
char_code('8',56).
char_code('9',57).
char_code(':',58).
char_code(';',59).
char_code('<',60).
char_code('=',61).
char_code('>',62).
char_code('?',63).
char_code('@',64).
char_code('A',65).
char_code('B',66).
char_code('C',67).
char_code('D',68).
char_code('E',69).
char_code('F',70).
char_code('G',71).
char_code('H',72).
char_code('I',73).
char_code('J',74).
char_code('K',75).
char_code('L',76).
char_code('M',77).
char_code('N',78).
char_code('O',79).
char_code('P',80).
char_code('Q',81).
char_code('R',82).
char_code('S',83).
char_code('T',84).
char_code('U',85).
char_code('V',86).
char_code('W',87).
char_code('X',88).
char_code('Y',89).
char_code('Z',90).
char_code('[',91).
char_code('\\',92).
char_code(']',93).
char_code('^',94).
char_code('_',95).
char_code('`',96).
char_code('a',97).
char_code('b',98).
char_code('c',99).
char_code('d',100).
char_code('e',101).
char_code('f',102).
char_code('g',103).
char_code('h',104).
char_code('i',105).
char_code('j',106).
char_code('k',107).
char_code('l',108).
char_code('m',109).
char_code('n',110).
char_code('o',111).
char_code('p',112).
char_code('q',113).
char_code('r',114).
char_code('s',115).
char_code('t',116).
char_code('u',117).
char_code('v',118).
char_code('w',119).
char_code('x',120).
char_code('y',121).
char_code('z',122).
char_code('{',123).
char_code('|',124).
char_code('}',125).
char_code('~',126).
