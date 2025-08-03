/** @type {import('tailwindcss').Config} */
module.exports = {
  content: ["./data/**/*.html"],
  theme: {
    extend: {
      colors: {
        'orange-darker': '#197C28',
        'orange-darkest': '#197C28',
        'black-discord': '#121214',
        'black-hover': '#1D1D1E',
        'black-select': '#363638',
        'black-background': '#1A1A1E',
        'button-green': '#197C28'
      }
    },
  },
  plugins: [],
}
