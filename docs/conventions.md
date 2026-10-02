# Conventions

## load_type

`load_type` is a function provided by `puerts-godot` to load a [static-binding](static-binding.md) type or a dynamic type from [ClassDB](https://docs.godotengine.org/en/stable/classes/class_classdb.html).

```javascript
const Vector2 = load_type("Vector2");
const v = new Vector2();
v.x = 8.0;
v.y = 6.0;
v.length() === 10.0 // true

const GlobalScope = load_type("GlobalScope");
GlobalScope.sin(1)
```

## to_callable

`to_callable` converts a script function to a Godot `Callable`.

```javascript
const callback = to_callable((message) => {
	log_info(message);
});

callback.call("hello");
```

## Enum / Signal

We treat enum as a static nested class and signal as a read-only property of the class.

```javascript
Vector2 = load_type("Vector2");
Vector2.Axis && Vector2.Axis.AXIS_X === 0 && Vector2.Axis.AXIS_Y === 1;
```

```javascript
const Timer = load_type("Timer");
const timer = new Timer();
timer.timeout.connect(to_callable(() => {
	log_info("timeout");
}));
// Note: You may need save the function reference to reuse it.
timer.start(1.0);
```

## Global Scope

We treat global scope as a static class with static methods, properties, and enums as static members.

```javascript
const GlobalScope = load_type("GlobalScope");
GlobalScope.sin(1)
```

## Operator Overloading

JavaScript does not support operator overloading, so we treat operator overloads as normal methods with a special name. e.g. `op_Addition`

See [index.js](../tools/puerts-godot-operator-model/index.js) for the operator name mapping.

```javascript
const Vector2 = load_type("Vector2");
const sum = Vector2.op_Addition(new Vector2(1.0, 2.0), new Vector2(3.0, 4.0));
sum.x === 4.0 && sum.y === 6.0;
```

## Constant

We treat constant as a static read-only property of the class. While get the value, we will create a new instance representing the constant value.

```javascript
const Vector2 = load_type("Vector2");
const v = Vector2.ZERO;
v.x === 0.0 && v.y === 0.0;
```
