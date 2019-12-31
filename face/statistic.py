
import numpy as np
import math

x1 = [1, 4, 6, 8, -3, 9, 0, -1]
x2 = [12, -8, 3, 9, -4, -6, 3, -7, -1,  0]
x = x1 + x2

n1 = len(x1)
m1 = sum(x1) / n1
s1 = math.sqrt(sum([(x - m1) ** 2 for x in x1]) / n1)
print(n1, m1, s1, np.std(x1))

n2 = len(x2)
m2 = sum(x2) / n2
s2 = math.sqrt(sum([(x - m2) ** 2 for x in x2]) / n2)
print(n2, m2, s2, np.std(x2))

n = len(x)
m = sum(x) / n
s = math.sqrt(sum([(x_ - m) ** 2 for x_ in x]) / n)
print(n, m, s, np.std(x))

n = n1 + n2
m = (n1 * m1 + n2 * m2) / n
s = math.sqrt((2 * m1 * n1 * (m1 - m) + 2 * m2 * n2 * (m2 - m) + n * m ** 2 - n1 * m1 ** 2 - n2 * m2 ** 2 + s1 ** 2 * n1 + s2 ** 2 * n2) / n)
print(n, m, s)

n = n1 + n2
m = (n1 * m1 + n2 * m2) / n
Sx1, Sx2  = n1 * m1, n2 * m2
a, b = n1 * s1 ** 2, n2 * s2 ** 2
c = Sx1 * 2 * (m1 - m) + Sx2 * 2 * (m2 - m) + n * m ** 2 - n1 * m1 ** 2 - n2 * m2 ** 2 + a + b
s = math.sqrt(c / n) 
print(n, m, s)
