-- Script con múltiples vulnerabilidades

-- 1. Uso de SQL dinámico sin sanitizar, permitiendo SQL Injection
CREATE OR REPLACE PROCEDURE fetch_user_data(user_id IN VARCHAR2) IS
    v_sql VARCHAR2(500);
    v_result VARCHAR2(500);
BEGIN
    v_sql := 'SELECT user_data FROM users WHERE user_id = ' || user_id;
    EXECUTE IMMEDIATE v_sql INTO v_result;
    DBMS_OUTPUT.PUT_LINE('Datos del usuario: ' || v_result);
END fetch_user_data;
/

-- 2. Uso de procedimientos almacenados sin autenticación
-- No verifica que el usuario esté autenticado antes de ejecutar acciones críticas
CREATE OR REPLACE PROCEDURE delete_user_account(user_id IN VARCHAR2) IS
BEGIN
    DELETE FROM users WHERE user_id = user_id;
    COMMIT;
    DBMS_OUTPUT.PUT_LINE('Cuenta de usuario eliminada');
END delete_user_account;
/

-- 3. Uso de contraseñas en texto plano
-- Las contraseñas no deberían almacenarse en texto plano
CREATE TABLE app_users (
    user_id VARCHAR2(50),
    password VARCHAR2(50)  -- Contraseña en texto plano (vulnerabilidad)
);
INSERT INTO app_users (user_id, password) VALUES ('usuario1', 'password123');
COMMIT;

-- 4. Uso de datos sensibles sin encriptación
-- Almacena datos sensibles sin encriptación
CREATE TABLE employee_salaries (
    employee_id NUMBER,
    salary NUMBER  -- Salario no cifrado (vulnerabilidad)
);
INSERT INTO employee_salaries (employee_id, salary) VALUES (101, 50000);
COMMIT;

-- 5. Error handling inadecuado
-- No se maneja adecuadamente las excepciones, permitiendo filtración de información
CREATE OR REPLACE PROCEDURE fetch_employee_salary(employee_id IN NUMBER) IS
    v_salary NUMBER;
BEGIN
    SELECT salary INTO v_salary FROM employee_salaries WHERE employee_id = employee_id;
    DBMS_OUTPUT.PUT_LINE('Salario del empleado: ' || v_salary);
EXCEPTION
    WHEN NO_DATA_FOUND THEN
        DBMS_OUTPUT.PUT_LINE('Empleado no encontrado');  -- Filtración de información
END fetch_employee_salary;
/

-- 6. Sin uso de límites de permisos
-- No restringe permisos para los procedimientos almacenados
CREATE OR REPLACE PROCEDURE get_all_user_data IS
    CURSOR user_cursor IS SELECT * FROM users;
    user_rec users%ROWTYPE;
BEGIN
    OPEN user_cursor;
    LOOP
        FETCH user_cursor INTO user_rec;
        EXIT WHEN user_cursor%NOTFOUND;
        DBMS_OUTPUT.PUT_LINE('Usuario: ' || user_rec.user_id || ', Datos: ' || user_rec.user_data);
    END LOOP;
    CLOSE user_cursor;
END get_all_user_data;
/
