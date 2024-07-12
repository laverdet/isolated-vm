<!--
Please only submit complete issues and questions. YOU ARE A SOFTWARE ENGINEER! Ask yourself: "if
someone was reporting an issue about something that I wrote what information would be helpful?".
That is, please include your nodejs version, your operating system version, your compiler version,
what you've tried so far to fix the issue (especially this one!).. things like that.

If some specific code is triggering the issue then please include a FULL EXAMPLE of the code that
can be run on its own. Please no "snippets", if I can't run the code without modifications then I
can't help you.
-->

## Is Your Question Already Answered?
- [ ] I have read the [Frequently Asked Question](https://github.com/laverdet/isolated-vm?tab=readme-ov-file#frequently-asked-question) in the README. My question is not the Frequently Asked Question.

## Personal Diagnostics
Please answer the following questions:

#### JavaScript includes a `setTimeout` function:
- [ ] Yes
- [ ] No

#### Functions are a type of primitive value in JavaScript:
- [ ] Yes
- [ ] No

#### Objects can be shared between isolates:
- [ ] Yes
- [ ] No

## The Code
- [ ] This code will parse and evaluate if I put it into a file called `main.mjs` and then run `node main.mjs`.

```js
// Replace this code with the code which demonstrates what you are asking about
import ivm from "isolated-vm";
const isolate = new ivm.Isolate();
const context = await isolate.createContext();
console.log(await context.eval('"hello world"'));
```
