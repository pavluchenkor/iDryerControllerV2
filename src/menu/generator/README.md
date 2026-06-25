# Генератор меню

## Cоздать виртуальное окружение 

python3 -m venv .venv
source .venv/bin/activate
pip install pyyaml

## Установка
```bash
pip install pyyaml
```
## Запуск
python src/menu/generator/gen_menu.py src/menu/menu.yaml --out src/menu

python src/menu/generator/gen_menu_v2.py src/menu/menu_v2.yaml --out src/menu

python src/menu/generator/gen_menu_v2.py src/menu/menu_v2.yaml --out src/menu --num-units 1