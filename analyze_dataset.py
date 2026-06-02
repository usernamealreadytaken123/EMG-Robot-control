import pandas as pd

# Загрузка набора признаков
df = pd.read_csv("features_dataset.csv")

# Посчитаем количество примеров каждого жеста
gesture_counts = df['gesture'].value_counts()
print("Количество примеров каждого жеста:")
print(gesture_counts)
