i = iter(iter((1, 2, 3)))
print(list(i))
i = iter(iter([1, 2, 3]))
print(list(i))
i = iter(iter({1:2, 3:4, 5:6}))
print(sorted(i))
i = iter(iter({1, 2, 3}))
print(list(i))
