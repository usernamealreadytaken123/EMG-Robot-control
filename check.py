import numpy as np
from sklearn.metrics import confusion_matrix, classification_report, accuracy_score
import seaborn as sns
import matplotlib.pyplot as plt

# Пример данных — замени своими
y_true = [0, 1, 2, 0, 1, 2, 3, 0, 2, 3, 1, 0, 3, 2, 1, 0, 2, 3]
y_pred = [0, 1, 2, 2, 1, 1, 3, 0, 2, 2, 0, 0, 3, 2, 3, 2, 2, 3]

# Расчёт метрик
conf_matrix = confusion_matrix(y_true, y_pred)
acc = accuracy_score(y_true, y_pred)
report = classification_report(y_true, y_pred, digits=2)

# Вывод метрик
print(f"Accuracy: {acc:.2f}\n")
print("Classification Report:\n", report)

# Отрисовка матрицы ошибок
plt.figure(figsize=(6, 5))
sns.heatmap(conf_matrix, annot=True, fmt="d", cmap="Blues", cbar=False,
            xticklabels=[0, 1, 2, 3], yticklabels=[0, 1, 2, 3])
plt.xlabel("Предсказанный класс")
plt.ylabel("Истинный класс")
plt.title("Матрица ошибок")
plt.tight_layout()
plt.show()
