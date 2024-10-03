-- Crear tabla de ejemplo
CREATE TABLE usuarios (
    id NUMBER PRIMARY KEY,
    nombre VARCHAR2(50),
    password VARCHAR2(50),
    saldo NUMBER
);

-- Vulnerabilidad de SQL Injection
-- Se espera que esta consulta reciba un id de usuario, pero permite la inyección de código SQL
PROCEDURE get_usuario_saldo(p_id IN VARCHAR2) IS
    v_saldo NUMBER;
    v_sql   VARCHAR2(4000);
BEGIN
    v_sql := 'SELECT saldo FROM usuarios WHERE id = ' || p_id;
    EXECUTE IMMEDIATE v_sql INTO v_saldo;
    DBMS_OUTPUT.PUT_LINE('El saldo del usuario es: ' || v_saldo);
EXCEPTION
    WHEN OTHERS THEN
        DBMS_OUTPUT.PUT_LINE('Error al obtener el saldo.');
END get_usuario_saldo;
/

-- Vulnerabilidad por manejo inseguro de transacciones
-- Este bloque no tiene manejo adecuado de errores y deja la transacción en estado inconsistente en caso de fallo.
PROCEDURE actualizar_saldo(p_id IN NUMBER, p_monto IN NUMBER) IS
BEGIN
    UPDATE usuarios
    SET saldo = saldo - p_monto
    WHERE id = p_id;
    
    -- Supongamos que aquí ocurre un error, pero no hay ROLLBACK.
    -- Esto puede dejar la base de datos en un estado inconsistente.
    COMMIT;
EXCEPTION
    WHEN OTHERS THEN
        DBMS_OUTPUT.PUT_LINE('Error al actualizar el saldo.');
        -- Falta un ROLLBACK aquí.
END actualizar_saldo;
/

-- Falta de encriptación de datos sensibles
-- El campo "password" no está encriptado ni protegido adecuadamente.
INSERT INTO usuarios (id, nombre, password, saldo) VALUES (1, 'usuario1', 'password123', 1000);
