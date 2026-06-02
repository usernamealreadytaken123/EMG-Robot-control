import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from sqlalchemy import create_engine

# Параметры подключения к PostgreSQL
user = 'postgres'
password = '123'
host = 'localhost'
port = 5432
db_name = 'emg_db'

# Загрузка данных


df = pd.read_csv("emg_data.csv")
df['value_mv'] = df['value'] * 5.0 / 1023 * 1000  # переводим в милливольты
# Выпрямление сигнала
df['abs_value'] = np.abs(df['value_mv'])

# Сглаживание (скользящее среднее)
df['smoothed'] = df['abs_value'].rolling(window=10).mean()

# Признаки
df['rms'] = df['abs_value'].rolling(window=50).apply(lambda x: np.sqrt(np.mean(x**2)), raw=True)
df['mav'] = df['abs_value'].rolling(window=50).mean()

# ZC — переходы через среднее
def zero_crossing(x):
    mean = np.mean(x)
    return np.sum(np.diff((x > mean).astype(int)) != 0)

df['zc'] = df['abs_value'].rolling(window=50).apply(zero_crossing)

# === ПОСТРОЕНИЕ ОТДЕЛЬНЫХ ГРАФИКОВ ===

# 1. Сырой сигнал
plt.figure(figsize=(10, 4))
plt.plot(df['timestamp'], df['value_mv'], color='blue')
plt.title('Сырой ЭМГ-сигнал')
plt.xlabel('Время')
plt.ylabel('Амплитуда')
plt.grid()
plt.tight_layout()
plt.show()

# 2. Сглаженный сигнал
plt.figure(figsize=(10, 4))
plt.plot(df['timestamp'], df['smoothed'], color='orange')
plt.title('Сглаженный сигнал (скользящее среднее)')
plt.xlabel('Время')
plt.ylabel('Амплитуда')
plt.grid()
plt.tight_layout()
plt.show()

# 3. RMS сигнала
plt.figure(figsize=(10, 4))
plt.plot(df['timestamp'], df['rms'], color='green')
plt.title('RMS сигнала')
plt.xlabel('Время')
plt.ylabel('RMS')
plt.grid()
plt.tight_layout()
plt.show()

# === СОХРАНЕНИЕ В БД ===
engine = create_engine(f'postgresql://{user}:{password}@{host}:{port}/{db_name}')
df_to_save = df[['timestamp', 'value', 'abs_value', 'smoothed', 'rms', 'mav', 'zc']].dropna()
df_to_save.columns = ['timestamp', 'raw_value', 'abs_value', 'smoothed', 'rms', 'mav', 'zc']
df_to_save.to_sql('emg_features', engine, if_exists='append', index=False)

print("Данные успешно записаны в базу PostgreSQL.")
