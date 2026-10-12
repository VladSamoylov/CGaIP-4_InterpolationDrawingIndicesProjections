#include <gl/glew.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <iostream>

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> distrib(1, 4);
int randomNumber = 1;

const GLint WIDTH = 800, HEIGHT = 800; // Константи початкової ширини та висоти вікна
GLuint shader, uniformModel, IBO, uniformProjection; // Глобальні змінні для об'єктів у пам'яті відеокарти shader - ID скомпільованої шейдерної програми, а uniformModel - ID для передачі матриці 4х4 у вершинний шейдер...
const float toRadians = 3.14159265f / 180.0f; // Коефіцієнт для переведення кутів із градусів у радіани
float curAngle = 0.0f; // Поточний кут повороту фігури
// Структура, що об'єднує дані одного об'єкта
struct Figure {
	GLuint VAO; // ID масиву налаштувань буферів
	GLuint VBO; // ID буфера з чистими координатами у відеопам'яті
	int verteciesAmount; // Реальна кількість вершин у фігурі
};
bool directionX = true; // Напрямок руху: true — вправо, false — вліво
bool directionY = true; // Напрямок руху: true — вгору, false — вниз
bool alive = true; // Стан фігури: true - фігура рухається, пульсує та обертається
float triOffsetX = 0.0f; // Поточне зміщення фігури по осі X
float triOffsetY = 0.0f; // Поточне зміщення фігури по осі Y
float impulse = 0.09f; // Поточна швідкість переміщення фігури
float impulseCur = 10.0f; // Поточна швидкість обертання фігури
float curSize = 0.4f; // Поточний розмір фігури

// Створюємо текст вершиного шейдера, який використовується для перетворення вершин у відповідні координати на екрані
static const char* vShader = R"(
	#version 330 // Вказуємо відеокарті, що код написаний на мові GLSL версії 3.3
	// in (це вхідні дані, які приходять у шейдер ззовні)
	// vec3 (координати у форматі вектора 3-х чисел типу float для кожної точки)
	// layout (location = 0) номер порту, який зв'язує C++ масив із цим шейдером
	layout(location = 0) in vec3 pos;  
	uniform mat4 model; // Створюємо Uniform-змінну для передачі матриці моделі 4х4
	void main()	{ // Головна функція шейдера, яка викликається окремо для кожної вершини
			gl_Position = model * vec4(pos, 1.0); // Змінна, в яку записується фінальна позиція точки на екрані, вона вимагає 3 координати (X, Y, Z) та 4 - перспективу 
	}
)";

// Створюємо текст фрагметного шейдера, який буде керувати кольором пікселей
static const char* fShader = R"(
	#version 330
	out vec4 colour; // out (вихідні дані, які шейдер відає назовні для подальшого відображення пікселю на екран
	uniform vec4 objectColor; // Створюємо Uniform-змінну для передачі кольору між C++ та відеокартою
		void main()	{ // Головна функція шейдера, яка викликається окремо для кожного пікселя
			colour = objectColor; // Керуємо коліром в форматі RGBA, присвоюємо пікселю той колір, який передасть C++
		}
)";

// Функція створення фігури
Figure CreateFigure(GLfloat* vertices, int size) {
	Figure newFig;
	newFig.verteciesAmount = size / (3 * sizeof(GLfloat)); // Рахуємо кількість вершин
	glGenVertexArrays(1, &newFig.VAO); // Створюємо VAO, яка буде запам'ятовувати всі налаштування буферів 
	glBindVertexArray(newFig.VAO); // Активуємо VAO
	glGenBuffers(1, &newFig.VBO); // Створюємо VBO буфер, куди фізично завантажимо координати фігури
	glBindBuffer(GL_ARRAY_BUFFER, newFig.VBO); // Активуємо VBO
	glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW); // Копіюємо масив точок з ОЗП у VRAM
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0); // Створюємо декодер-пояснення як читати байти (слот №0, 3 числа, типу флоат, без нормалізації, крок 0, зміщення 0)
	glEnableVertexAttribArray(0); // Вмикаємо декодер для слоту 0
	glBindBuffer(GL_ARRAY_BUFFER, 0); // Відв'язуємо буфери, щоб випадково не стерти дані в іншому місці програми
	glBindVertexArray(0);
	return newFig;
}

// Допоміжна функція для компіляції одного шейдера
void AddShader(GLuint theProgram, const char* shaderCode, GLenum shaderType)
{
	GLuint theShader = glCreateShader(shaderType); // Створюємо порожній об'єкт шейдера потрібного типу (вершинний або фрагментний)
	const GLchar* theCode[1];
	theCode[0] = shaderCode; // Передаємо текст програми нашого шейдера
	GLint codeLength[1];
	codeLength[0] = strlen(shaderCode); // Рахуємо довжину тексту
	glShaderSource(theShader, 1, theCode, codeLength); // Передаємо текст шейдера у створений порожній об'єкт шейдера
	glCompileShader(theShader); // Компілюємо шейдер
	// Перевіряємо на помилки в коді GLSL шейдера
	GLint result = 0;
	GLchar eLog[1024] = { 0 };
	glGetShaderiv(theShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(theShader, 1024, NULL, eLog);
		fprintf(stderr, "Error compiling the %d shader:	'%s'\n", shaderType, eLog); // Якщо компіляція провалилася до виводимо інформацію
		return;
	}
	glAttachShader(theProgram, theShader); // Інакше прикріплюємо цей скомпільований шейдер до нашої програми
}

void CompileShaders() {
	shader = glCreateProgram(); // Створюємо програму-інструкцію для відеокарти
	if (!shader)
	{
		printf("Failed to create shader\n");
		return;
	}
	AddShader(shader, vShader, GL_VERTEX_SHADER); // Додаємо та компілюємо вершинний шейдер
	AddShader(shader, fShader, GL_FRAGMENT_SHADER); // Додаємо та компілюємо фрагментний шейдер
	GLint result = 0;
	GLchar eLog[1024] = { 0 };
	glLinkProgram(shader); // З'єднує шейдер в одну інструкцію
	// Перевіряємо чи успішно вони з'єдналися між собою
	glGetProgramiv(shader, GL_LINK_STATUS, &result);
	if (!result)
	{
		glGetProgramInfoLog(shader, sizeof(eLog), NULL, eLog);
		printf("Error linking program: '%s'\n", eLog);
		return;
	}
	// Перевіряємо, чи зможе взагалі цей шейдер запуститися на поточній відеокарті
	glValidateProgram(shader);
	glGetProgramiv(shader, GL_VALIDATE_STATUS, &result);
	if (!result)
	{
		glGetProgramInfoLog(shader, sizeof(eLog), NULL, eLog);
		printf("Error validating program: '%s'\n", eLog);
		return;
	}
	uniformModel = glGetUniformLocation(shader, "model"); // находимо адресу змінної "model" всередині шейдеру і записуємо її в глобальний ідентифікатор uniformModel
}

// Функція, яка повертає коефіцієнт руху по осі Х
float GetDirectionX() {
	if (directionX) return 1.0f;
	else return -1.0f;
}

// Функція, яка повертає коефіцієнт руху по осі У
float GetDirectionY() {
	if (directionY) return 1.0f;
	else return -1.0f;
}

int main() {
	if (!glfwInit()) // Спроба ініціалізації бібліотеки GLFW, яка потрібна для створення віконих інтерфейсів та обробки подій в графічних програмах
	{
		printf("GLFW failed to start :(");
		glfwTerminate(); // У разі не ініціалізації бібліотеки GLFW виконується очищення ресурсів і завершення виконання програми
		return 1;
	}
	// Налаштовуємо OpenGL перед створенням вікна
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Встановлюємо головну версію OpenGL 
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Встановлюємо додаткову версію OpenGL 
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Використовуємо Core-профіль, профіль сучасного OpenGL без застарілих функцій
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Вмикаємо зворотну сумісність для коректної роботи з macOS та новими драйверами
	GLFWwindow* mainWindow = glfwCreateWindow(WIDTH, HEIGHT, "by Vladislav Samoilov KNT-223", NULL, NULL); // Створюємо головне вікно програми із заданими розмірами та заколовком
	if (!mainWindow) // Перевіряємо ініціалізацію вікна
	{
		printf("GLFW window creation failed :(");
		glfwTerminate(); // Закриваємо GLFW, якщо вікно не вдалося створити та завершаємо виконання програми
		return 1;
	}
	int bufferWidth, bufferHeight; // Змінні для збереження реального розміру області рендерингу (в пікселях)
	glfwGetFramebufferSize(mainWindow, &bufferWidth, &bufferHeight); // Отримуємо фактичний розмір буфера кадру вікна
	glfwMakeContextCurrent(mainWindow); // Робимо контекст OpenGL створеного вікна поточним для цього потоку програми
	glewExperimental = GL_TRUE; // Дозволяємо GLEW завантажувати експериментальні та нові розширення OpenGL
	if (glewInit() != GLEW_OK) // Перевіряємо ініціалізацію бібліотеки GLEW, який динамічно завантажує функції OpenGL для відеокарти
	{
		printf("GLEW failed to start >_<");
		glfwDestroyWindow(mainWindow); // Знищуємо створене вікно
		glfwTerminate(); // Завершуємо роботу GLFW
		return 1;
	}
	glViewport(0, 0, bufferWidth, bufferHeight); // Налаштовуємо область перегляду OpenGL
	// Координати вершин прямокутника
	GLfloat squareV[] = {
		-0.5f, -0.4f, 0.0f, // Лівий нижній кут
		-0.5f,  0.4f, 0.0f, // Лівий верхній кут
		0.5f, -0.4f, 0.0f, // Правий нижній кут
		-0.5f,  0.4f, 0.0f, // Лівий верхній кут 
		0.5f,  0.4f, 0.0f, // Правий верхній кут
		0.5f, -0.4f, 0.0f  // Правий нижній кут
	};

	Figure squre = CreateFigure(squareV, sizeof(squareV)); // Створюємо геометрію прямокутника 
	CompileShaders(); // Компілюємо шейдери
	GLint colorLoc = glGetUniformLocation(shader, "objectColor"); // Знаходимо змінну "objectColor" у пам'яті шейдер
	while (!glfwWindowShouldClose(mainWindow)) // Головний цикл роботи програми, працює, поки користувач не закриє вікно
	{
		glfwPollEvents(); // Перевіряємо та обробляємо події
		impulse *= 0.999f; // В кожному кадрі зменшуємо швидкість руху фигури на 0.1% 
		if (impulse < 0.0001f) { impulse = 0.0f; alive = false; } // переводимо фігуру в стан "мертва" якщо швидкість руху фігури занадто мала
		switch (randomNumber) { // Вибір напрямку руху фігури на основі рандомного числа
		case 1:
			triOffsetX += GetDirectionX() * impulse; // Рух по горизонталі
			break;
		case 2:
			triOffsetX += GetDirectionX() * impulse; // Рух по діагоналі
			triOffsetY += GetDirectionY() * impulse;
			break;
		case 3:
			triOffsetY += GetDirectionY() * impulse; // Рух по вертикалі
			break;
		case 4:
			triOffsetX += GetDirectionX() * impulse; // Рух по діагоналі
			triOffsetY += GetDirectionY() * impulse;
			break;
		}
		float dynamicMaxOffset = 1.0f - (curSize * 0.5f); // Розраховуємо невидиму стіну, використовуючи розмір поточної фігури
		if (abs(triOffsetX) >= dynamicMaxOffset) { // Якщо фігура зіткнулася зі стіною по оси Х
			if (triOffsetX > 0) triOffsetX = dynamicMaxOffset; // Якщо фігура зіштовхнулася з правою стіною, то фігуру відштовхуємо від стіни
			else triOffsetX = -dynamicMaxOffset; // Інакше фігура зіштовхнулася з лівою стіною
			directionX = !directionX; // Якщо вперлися межу екрану, тоді міняємо напрямок по оси Х на протилежний
			randomNumber = distrib(gen); // Генеруємо новий напрямок руху фігури при зіткненні зі стіною
		}
		if (abs(triOffsetY) >= dynamicMaxOffset) { // Якщо фігура зіткнулася зі стіною по оси У
			if (triOffsetY > 0) triOffsetY = dynamicMaxOffset; // Якщо фігура зіштовхнулася зі стелею, то фігуру відштовхуємо від стіни
			else triOffsetY = -dynamicMaxOffset; // Інакше фігура зіштовхнулася з підлогою
			directionY = !directionY; // Якщо вперлися межу екрану, тоді міняємо напрямок по оси Уна протилежний
			randomNumber = distrib(gen); // Генеруємо новий напрямок руху фігури при зіткненні зі стіною
		}
		impulseCur *= 0.999f; // В кожному кадрі зменшуємо швидкість повороту фигури на 0.1% 
		curAngle += impulseCur; // На кожному кадрі збільшуємо кут повороту
		if (curAngle >= 360) {
			curAngle = 0; // Обнуляємо кут при повному оберті, щоб число не росло до нескінченності
		}
		float time = (float)glfwGetTime(); // Отримуємо точний час роботи програми в секундах від GLFW
		float heartImpulse = powf(sinf(time * 6.0f), 6.0f); // Створюємо ефект різкого поштовху та довгого спокою 
		if (alive) curSize = 0.2f + 0.1f * heartImpulse; // Поки фігура "жива", вона активно пульсує в межах від 0.2 до 0.3
		else curSize = 0.2f;
		glClearColor(0.0f, 0.3f, 1.0f, 1.0f); // Встановлюємо колір для очищення екрану RGBA
		glClear(GL_COLOR_BUFFER_BIT); // Очищаємо буфер кольору кадру за допомогою вказаного вище кольору
		glUseProgram(shader); // Вмикаємо шейдерну програму
		if (alive) glUniform4f(colorLoc, 1.0f, 0.0f, 0.0f, 1.0f); // Передаємо значення коліру до шейдеру
		else glUniform4f(colorLoc, 0.5f, 0.5f, 0.5f, 1.0f); // Передаємо значення коліру до шейдеру
		glm::mat4 model(1.0f); // Ініціалізація одиничної матриці моделі
		model = glm::translate(model, glm::vec3(triOffsetX, triOffsetY, 0.0f)); // Додаємо зміщення по осях Х та У
		model = glm::rotate(model, curAngle * toRadians, glm::vec3(0.0f, 0.0f, 1.0f)); // Додаємо обертання об'єкта навколо осі Z
		model = glm::scale(model, glm::vec3(curSize, curSize, 1.0f)); // Додаємо масштаб, множимо осі X та Y на коефіцієнт curSize
		glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(model)); // Передаємо готову фінальну матрицю 4х4 у вершинний шейдер без перевороту (GL_FALSE)

		glBindVertexArray(squre.VAO); // Вмикаємо налаштування фігури
		glDrawArrays(GL_TRIANGLES, 0, squre.verteciesAmount); // Даємо команду намалювати фігуру прямокутник

		// Очищення, для того, щоб інші об'єкти не малювалися цим шейдером
		glBindVertexArray(0);
		glUseProgram(0);
		glfwSwapBuffers(mainWindow); // Міняємо місцями задній буфер (де ми малювали) і передній буфер (який бачить користувач)
	}
	return 0;
}