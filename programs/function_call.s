MOVZ X0, #7
MOVZ X1, #5

BL add_numbers

B end

add_numbers:
ADD X0, X0, X1
RET

end: