#pragma once

const std::string builtin_list = R"(
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
)";
