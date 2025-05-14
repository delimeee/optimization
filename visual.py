import matplotlib.pyplot as plt

# Пути к входным данным
power_plant_file = "./power_plant.txt"
home_file = "./home.txt"
solution_file = "./solution_base.txt"

def visualize_energy_grid():
    # Чтение координат и мощности всех узлов (станции и города)
    nodes = {}
    try:
        with open(power_plant_file, 'r') as f:
            n = int(f.readline())
            for i in range(n):
                x, y, p = map(float, f.readline().split())
                nodes[i] = {'x': x, 'y': y, 'power': p, 'is_station': True}
    except Exception as e:
        print("Ошибка при чтении power_plant.txt:", e)
        return

    try:
        with open(home_file, 'r') as f:
            m = int(f.readline())
            for i in range(m):
                x, y, d = map(float, f.readline().split())
                nodes[n + i] = {'x': x, 'y': y, 'power': -d, 'is_station': False}
    except Exception as e:
        print("Ошибка при чтении home.txt:", e)
        return

    # Чтение рёбер и потоков из файла решения
    edges = []
    try:
        with open(solution_file, 'r') as f:
            for line in f:
                a, b, lines, flow = line.strip().split()
                edges.append({'from': int(a), 'to': int(b), 'lines': int(lines), 'flow': float(flow)})
    except Exception as e:
        print("Ошибка при чтении solution файла:", e)
        return

    # Определяем, какие узлы используются в решении
    used_nodes = set()
    for edge in edges:
        used_nodes.add(edge['from'])
        used_nodes.add(edge['to'])

    # Создание изображения
    plt.figure(figsize=(14, 10))

    # Отображение линий между узлами
    for edge in edges:
        a, b = edge['from'], edge['to']
        x1, y1 = nodes[a]['x'], nodes[a]['y']
        x2, y2 = nodes[b]['x'], nodes[b]['y']
        flow = edge['flow']

        # Толщина линии зависит от потока, но ограничена
        lw = min(5.0, max(0.5, flow / 150))
        color = 'red' if flow > 800 else 'black'

        # Если поток = 0, рисуем серым пунктиром
        if flow == 0:
            plt.plot([x1, x2], [y1, y2], color='gray', linewidth=1.2, linestyle='dashed', alpha=0.6)
        else:
            plt.plot([x1, x2], [y1, y2], color=color, linewidth=lw, alpha=0.8)

        # Подпись потока, если он больше 0
        mx, my = (x1 + x2) / 2, (y1 + y2) / 2
        if flow > 0:
            plt.text(mx, my + 10, f"{int(flow)} MW", fontsize=8,
                     ha='center', va='bottom', backgroundcolor='white')

    # Отображение узлов: станции (используются/нет), города
    for node_id, data in nodes.items():
        x, y = data['x'], data['y']
        power = int(data['power'])
        is_station = data['is_station']

        if is_station:
            if node_id in used_nodes:
                # Активная станция: красный квадрат
                plt.scatter(x, y, c='red', s=130, marker='s', zorder=3)
            else:
                # Неиспользуемая станция: серая заливка с пунктирной рамкой
                plt.scatter(x, y, facecolors='lightgray', edgecolors='gray',
                            s=130, marker='s', zorder=3, linewidths=1.5, linestyle='dashed')
        else:
            # Город: синий круг
            plt.scatter(x, y, c='blue', s=100, marker='o', zorder=3)

        # Подпись узла (номер и мощность)
        label = f"{node_id}\n({power})"
        plt.text(x + 5, y + 5, label, fontsize=8)

    # Оформление графика
    plt.title("Energy Grid Visualization", fontsize=14)
    plt.xlabel("X (km)")
    plt.ylabel("Y (km)")
    plt.axis("equal")
    plt.grid(True)
    plt.tight_layout()

    # Сохранение и показ
    plt.savefig("final_energy_grid.png", dpi=300)
    plt.show()

if __name__ == "__main__":
    visualize_energy_grid()
